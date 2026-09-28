"""Tests for scripts/pio-free-demand.py.

Required by the `PIO_FREE-title-demand-bound-r1` discovery, which specifies fixtures for
positive A1 and ModRM disp32 encodings, operand-vs-instruction correction, variable eax
AND ecx compare registers, masked constants other than 4, and negatives for malformed or
truncated input, changed VA/population, unknown instruction or threshold form, a
missing/duplicate site, a mid-instruction start that must be REJECTED rather than decoded
as plausible garbage, and a known-bad hex-spelling-only undercount.

The mid-instruction case is the important one: an earlier session's ad-hoc disassembly
began one byte early and capstone silently produced plausible `.byte`/garbage output.  The
test below encodes that exact hazard as a fixture and requires rejection.

Run:  python -X utf8 -m unittest scripts.test_pio_free_demand
"""
from __future__ import annotations

import importlib.util
import json
import struct
import tempfile
import unittest
from pathlib import Path

import capstone

ROOT = Path(__file__).resolve().parents[1]

# The tool's filename contains a hyphen, so it cannot be imported by name.
_spec = importlib.util.spec_from_file_location(
    "pio_free_demand", ROOT / "scripts" / "pio-free-demand.py")
pfd = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(pfd)

PIO = pfd.PIO_FREE_VA


def _md():
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    return md


class FixtureXbe:
    """A tiny synthetic XBE with one section, so encodings can be tested in isolation."""

    def __init__(self, code: bytes, va: int = 0x00100000):
        self.code = code
        self.va = va

    def __enter__(self):
        self._dir = tempfile.TemporaryDirectory()
        d = Path(self._dir.name)
        self.xbe = d / "default.xbe"
        self.analysis = d / "mygame_analysis.json"
        # raw_addr 0 so file offsets equal section offsets.
        self.xbe.write_bytes(self.code)
        self.analysis.write_text(json.dumps({"sections": [{
            "name": ".text",
            "virtual_addr": f"0x{self.va:08X}",
            "raw_addr": "0x00000000",
            "raw_size": len(self.code),
        }]}), encoding="utf-8")
        return self

    def __exit__(self, *exc):
        self._dir.cleanup()
        return False


def a1_moffs_read(reg_free_va: int = PIO) -> bytes:
    """`A1` + disp32: mov eax, [disp32].  Operand sits at start+1."""
    return b"\xA1" + struct.pack("<I", reg_free_va)


def modrm_disp32_read(reg_field: int = 2, va: int = PIO) -> bytes:
    """`8B /r` with mod=00, r/m=101: mov r32, [disp32].  Operand sits at start+2."""
    modrm = (0b00 << 6) | ((reg_field & 7) << 3) | 0b101
    return b"\x8B" + bytes([modrm]) + struct.pack("<I", va)


class EncodingTests(unittest.TestCase):
    def test_a1_moffs_positive(self):
        code = a1_moffs_read() + b"\x25\xFC\xFF\xFF\xFF" + b"\x83\xF8\x04" + b"\x72\xF2"
        with FixtureXbe(code) as fx:
            sections = pfd.load_sections(fx.analysis)
            md = _md()
            # the operand dword is at instruction_va + 1
            located = pfd.locate_read(fx.xbe, sections, fx.va + 1, md)
            self.assertEqual(located["encoding"], pfd.ENC_A1_MOFFS)
            self.assertEqual(located["instruction_va"], fx.va)
            self.assertEqual(located["loaded_register"], "eax")
            row = pfd.classify_site(located)
            self.assertEqual(row["gate_form"], "CONSTANT")
            self.assertEqual(row["demand_literal"], "0x00000004")

    def test_modrm_disp32_positive(self):
        code = modrm_disp32_read(2) + b"\x81\xE2\xFC\xFF\xFF\xFF" + b"\x3B\xCA" + b"\x72\xF2"
        with FixtureXbe(code) as fx:
            sections = pfd.load_sections(fx.analysis)
            md = _md()
            located = pfd.locate_read(fx.xbe, sections, fx.va + 2, md)
            self.assertEqual(located["encoding"], pfd.ENC_MODRM_DISP32)
            self.assertEqual(located["instruction_va"], fx.va)
            self.assertEqual(located["loaded_register"], "edx")

    def test_mid_instruction_start_is_rejected_not_decoded(self):
        """The tool must resolve the instruction start by DECODING, never by assuming.

        This reproduces the exact hazard that produced plausible garbage in an earlier
        session.  A two-byte `xor eax,eax` precedes an `A1` PIO_FREE load, so:

            va+0  33 C0        xor eax, eax
            va+2  A1 10 00 82 FE   mov eax, [0xFE820010]   <- operand VA is va+3

        A caller that wrongly applied the ModRM convention would derive the start as
        `operand_va - 2 == va+1`, which is the SECOND BYTE of the `xor` -- a
        mid-instruction start.  Decoding from there is exactly the case that can yield
        plausible garbage instead of an error.

        The tool must (a) find the correct start `va+2` via the A1 encoding, and (b) never
        report `va+1` as the instruction start.
        """
        code = (b"\x33\xC0"                                  # xor eax, eax
                + a1_moffs_read()                            # A1 + disp32 at va+2
                + b"\x25\xFC\xFF\xFF\xFF"                    # and eax, 0xFFFFFFFC
                + b"\x83\xF8\x04"                            # cmp eax, 4
                + b"\x72\xF2")                               # jb back
        with FixtureXbe(code) as fx:
            sections = pfd.load_sections(fx.analysis)
            md = _md()
            operand_va = fx.va + 2 + 1          # the A1 operand sits at instruction+1

            located = pfd.locate_read(fx.xbe, sections, operand_va, md)
            self.assertEqual(located["instruction_va"], fx.va + 2)
            self.assertEqual(located["encoding"], pfd.ENC_A1_MOFFS)
            # and it must NOT have taken the mid-instruction start
            self.assertNotEqual(located["instruction_va"], fx.va + 1)
            # the instruction really is the A1 load, decoded from a verified boundary
            self.assertEqual(located["instruction_bytes"][:2], "a1")

            # A caller who mis-derives the operand VA by one must be REJECTED, not
            # silently given a different instruction.
            with self.assertRaises(pfd.ClassifyError):
                pfd.locate_read(fx.xbe, sections, operand_va + 1, md)

    def test_wrong_operand_va_for_a1_site_is_rejected(self):
        """operand_va + 1 for an A1 site must not resolve to any valid read."""
        code = a1_moffs_read() + b"\x25\xFC\xFF\xFF\xFF" + b"\x83\xF8\x04" + b"\x72\xF2"
        with FixtureXbe(code) as fx:
            sections = pfd.load_sections(fx.analysis)
            with self.assertRaises(pfd.ClassifyError):
                pfd.locate_read(fx.xbe, sections, fx.va + 2, _md())

    def test_operand_va_not_a_read_of_pio_free_is_rejected(self):
        """An operand pointing at a DIFFERENT address must not classify as a PIO site."""
        code = a1_moffs_read(0xFE820014) + b"\x83\xF8\x04" + b"\x72\xF2"
        with FixtureXbe(code) as fx:
            sections = pfd.load_sections(fx.analysis)
            with self.assertRaises(pfd.ClassifyError):
                pfd.locate_read(fx.xbe, sections, fx.va + 1, _md())


class GateFormTests(unittest.TestCase):
    def _classify(self, code, operand_off):
        with FixtureXbe(code) as fx:
            sections = pfd.load_sections(fx.analysis)
            located = pfd.locate_read(fx.xbe, sections, fx.va + operand_off, _md())
            return pfd.classify_site(located)

    def test_variable_demand_in_ecx(self):
        # mov edx,[PIO]; shr edx,2; cmp edx,ecx; jb back
        # `cmp edx,ecx` puts the POLLED edx in operand 0, so compare_order is poll_first
        # and the demand register (operand 1) is ecx.
        code = (modrm_disp32_read(2) + b"\xC1\xEA\x02" + b"\x3B\xD1" + b"\x72\xF2")
        row = self._classify(code, 2)
        self.assertEqual(row["gate_form"], "VARIABLE")
        self.assertEqual(row["demand_register"], "ecx")
        self.assertEqual(row["compare_order"], "poll_first")
        self.assertTrue(row["unsigned_below"])
        self.assertEqual(row["transforms"][0]["op"], "shr")

    def test_variable_demand_in_eax(self):
        # mov edx,[PIO]; shr edx,2; cmp eax,edx; jb back
        # here the demand eax is operand 0 and the polled edx is operand 1, so the order
        # is poll_second -- the mirror of the case above.
        code = (modrm_disp32_read(2) + b"\xC1\xEA\x02" + b"\x3B\xC2" + b"\x72\xF2")
        row = self._classify(code, 2)
        self.assertEqual(row["gate_form"], "VARIABLE")
        self.assertEqual(row["demand_register"], "eax")
        self.assertEqual(row["compare_order"], "poll_second")

    def test_masked_constant_other_than_four(self):
        # mov eax,[PIO]; and eax,0xFFFFFFFC; cmp eax,0x4C; jb back
        code = (a1_moffs_read() + b"\x25\xFC\xFF\xFF\xFF" + b"\x83\xF8\x4C" + b"\x72\xF2")
        row = self._classify(code, 1)
        self.assertEqual(row["gate_form"], "CONSTANT")
        self.assertEqual(row["demand_literal"], "0x0000004C")
        self.assertEqual(row["stub_available"], "0x00000080")
        self.assertTrue(row["stub_passes"])
        self.assertEqual(row["stub_margin"], 128 - 0x4C)

    def test_constant_at_zero_margin(self):
        # the two real 0x80 sites: stub's masked value EQUALS the literal.
        code = (a1_moffs_read() + b"\x25\xFC\xFF\xFF\xFF" + b"\x3D\x80\x00\x00\x00" + b"\x72\xF2")
        row = self._classify(code, 1)
        self.assertEqual(row["demand_literal"], "0x00000080")
        self.assertTrue(row["stub_passes"])
        self.assertEqual(row["stub_margin"], 0)

    def test_constant_above_stub_fails(self):
        # a threshold above 128 must NOT pass.
        code = (a1_moffs_read() + b"\x25\xFC\xFF\xFF\xFF" + b"\x3D\x00\x01\x00\x00" + b"\x72\xF2")
        row = self._classify(code, 1)
        self.assertEqual(row["demand_literal"], "0x00000100")
        self.assertFalse(row["stub_passes"])
        self.assertLess(row["stub_margin"], 0)

    def test_unknown_threshold_form_is_rejected(self):
        """A `test`-style predicate is not a supported threshold form -> reject."""
        code = (a1_moffs_read() + b"\xA9\x00\x01\x00\x00" + b"\x74\xF2")
        with self.assertRaises(pfd.ClassifyError):
            self._classify(code, 1)

    def test_no_compare_is_rejected(self):
        code = a1_moffs_read() + b"\x90" * 20
        with self.assertRaises(pfd.ClassifyError):
            self._classify(code, 1)

    def test_mov_from_other_register_breaks_stub_chain(self):
        """If the value is moved from an unrelated register, the stub's contribution is
        not determined, so a CONSTANT gate must be rejected rather than guessed."""
        # mov edx,[PIO]; mov eax,ebx; cmp eax,4; jb back
        code = (modrm_disp32_read(2) + b"\x8B\xC3" + b"\x83\xF8\x04" + b"\x72\xF2")
        with self.assertRaises(pfd.ClassifyError):
            self._classify(code, 2)


class InputValidationTests(unittest.TestCase):
    def test_truncated_section_is_rejected(self):
        code = a1_moffs_read()[:3]  # too short to hold the operand
        with FixtureXbe(code) as fx:
            sections = pfd.load_sections(fx.analysis)
            with self.assertRaises(pfd.ClassifyError):
                pfd.locate_read(fx.xbe, sections, fx.va + 1, _md())

    def test_range_outside_section_is_rejected(self):
        code = a1_moffs_read()
        with FixtureXbe(code) as fx:
            sections = pfd.load_sections(fx.analysis)
            with self.assertRaises(pfd.ClassifyError):
                pfd.xbe_slice(fx.xbe, sections, 0x7F000000, 16)

    def test_missing_analysis_file_is_rejected(self):
        with FixtureXbe(a1_moffs_read()) as fx:
            with self.assertRaises(pfd.ClassifyError):
                pfd.load_sections(fx.analysis.parent / "nope.json")

    def test_malformed_analysis_is_rejected(self):
        with FixtureXbe(a1_moffs_read()) as fx:
            fx.analysis.write_text("{not json", encoding="utf-8")
            with self.assertRaises(pfd.ClassifyError):
                pfd.load_sections(fx.analysis)

    def test_analysis_without_sections_is_rejected(self):
        with FixtureXbe(a1_moffs_read()) as fx:
            fx.analysis.write_text(json.dumps({"sections": []}), encoding="utf-8")
            with self.assertRaises(pfd.ClassifyError):
                pfd.load_sections(fx.analysis)

    def test_section_missing_required_key_is_rejected(self):
        with FixtureXbe(a1_moffs_read()) as fx:
            fx.analysis.write_text(json.dumps({"sections": [
                {"name": ".text", "virtual_addr": "0x0"}]}), encoding="utf-8")
            with self.assertRaises(pfd.ClassifyError):
                pfd.load_sections(fx.analysis)

    def test_missing_xbe_is_rejected(self):
        with FixtureXbe(a1_moffs_read()) as fx:
            sections = pfd.load_sections(fx.analysis)
            with self.assertRaises(pfd.ClassifyError):
                pfd.find_operand_offsets(fx.xbe.parent / "nope.xbe", sections, PIO)


class PopulationTests(unittest.TestCase):
    def test_changed_population_changes_count(self):
        """A population change must show up as a count change, not be silently absorbed."""
        one = a1_moffs_read() + b"\x25\xFC\xFF\xFF\xFF" + b"\x83\xF8\x04" + b"\x72\xF2"
        two = one + a1_moffs_read() + b"\x25\xFC\xFF\xFF\xFF" + b"\x83\xF8\x04" + b"\x72\xF2"
        with FixtureXbe(one) as fx1:
            s1 = pfd.load_sections(fx1.analysis)
            self.assertEqual(len(pfd.find_operand_offsets(fx1.xbe, s1, PIO)), 1)
        with FixtureXbe(two) as fx2:
            s2 = pfd.load_sections(fx2.analysis)
            self.assertEqual(len(pfd.find_operand_offsets(fx2.xbe, s2, PIO)), 2)

    def test_duplicate_operand_offsets_are_both_reported(self):
        """Two identical sites are two sites; neither may be dropped."""
        site = a1_moffs_read() + b"\x25\xFC\xFF\xFF\xFF" + b"\x83\xF8\x04" + b"\x72\xF2"
        code = site + site
        with FixtureXbe(code) as fx:
            sections = pfd.load_sections(fx.analysis)
            hits = pfd.find_operand_offsets(fx.xbe, sections, PIO)
            self.assertEqual(len(hits), 2)
            self.assertNotEqual(hits[0][0], hits[1][0])


class GeneratedSpellingTests(unittest.TestCase):
    def test_both_spellings_found_and_hex_only_undercounts(self):
        """The known-bad hex-spelling-only scan must undercount; the normalising scan
        must not."""
        with tempfile.TemporaryDirectory() as d:
            gen = Path(d) / "gen"
            gen.mkdir()
            # hex spelling for an A1 moffs load
            (gen / "a.c").write_text("eax = MEM32(0xFE820010u);\n", encoding="utf-8")
            # signed-decimal spelling for a ModRM disp32 operand
            (gen / "b.c").write_text("edx = MEM32(-25034736);\n", encoding="utf-8")
            res = pfd.scan_generated(gen, PIO)
            self.assertEqual(res["total"], 2)
            self.assertEqual(res["by_spelling"], {"decimal": 1, "hex": 1})
            # prove the two spellings really are the same value
            self.assertEqual(pfd._u32(-25034736), PIO)

    def test_hex_only_scan_would_miss_the_decimal_sites(self):
        with tempfile.TemporaryDirectory() as d:
            gen = Path(d) / "gen"
            gen.mkdir()
            (gen / "b.c").write_text("edx = MEM32(-25034736);\n", encoding="utf-8")
            res = pfd.scan_generated(gen, PIO)
            self.assertEqual(res["total"], 1)
            self.assertEqual(res["by_spelling"], {"decimal": 1})

    def test_scan_rejects_missing_directory(self):
        with self.assertRaises(pfd.ClassifyError):
            pfd.scan_generated(Path("does/not/exist"), PIO)

    def test_scan_rejects_directory_without_c_files(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaises(pfd.ClassifyError):
                pfd.scan_generated(Path(d), PIO)


class DeterminismTests(unittest.TestCase):
    def test_repeat_runs_are_byte_identical(self):
        code = (a1_moffs_read() + b"\x25\xFC\xFF\xFF\xFF" + b"\x83\xF8\x04" + b"\x72\xF2"
                + modrm_disp32_read(2) + b"\xC1\xEA\x02" + b"\x3B\xD1" + b"\x72\xF2")
        with FixtureXbe(code) as fx:
            a = json.dumps(pfd.classify_all(fx.xbe, fx.analysis), indent=2)
            b = json.dumps(pfd.classify_all(fx.xbe, fx.analysis), indent=2)
            self.assertEqual(a, b)

    def test_sites_are_sorted_by_instruction_va(self):
        first = a1_moffs_read() + b"\x25\xFC\xFF\xFF\xFF" + b"\x83\xF8\x04" + b"\x72\xF2"
        second = modrm_disp32_read(2) + b"\xC1\xEA\x02" + b"\x3B\xD1" + b"\x72\xF2"
        with FixtureXbe(second + first) as fx:
            res = pfd.classify_all(fx.xbe, fx.analysis)
            vas = [int(s["instruction_va"], 16) for s in res["sites"]]
            self.assertEqual(vas, sorted(vas))

    def test_rejects_are_reported_not_dropped(self):
        good = a1_moffs_read() + b"\x25\xFC\xFF\xFF\xFF" + b"\x83\xF8\x04" + b"\x72\xF2"
        # a read of PIO_FREE followed by no compare at all -> must be a reject
        bad = modrm_disp32_read(3) + b"\x90" * 20
        with FixtureXbe(good + bad) as fx:
            res = pfd.classify_all(fx.xbe, fx.analysis)
            self.assertEqual(res["operand_offsets_found"], 2)
            self.assertEqual(res["sites_classified"], 1)
            self.assertEqual(res["sites_rejected"], 1)
            self.assertTrue(res["rejects"][0]["reason"])


if __name__ == "__main__":
    unittest.main()
