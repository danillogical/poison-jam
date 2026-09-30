"""Tests for `scripts/enumerate-accesses.py` (plan T9).

The plan's PASS criterion is behavioural, not structural: the tool must reproduce
`PIO_FREE` = 28 sites (10 hex + 18 signed-decimal spellings) and the vtable base = 3
references, and every run must print its own known-answer controls.

The load-bearing tests are the ones that encode the *failure this tool exists to prevent*:

  * `test_hex_spelling_only_would_undercount` -- the recorded 10-of-28 undercount;
  * `test_signed_displacement_normalises_to_uint32` -- `-25034736 == 0xFE820010`;
  * `test_unaligned_immediate_is_found` -- aligned dword scans miss unaligned immediates;
  * `test_indirect_call_keeps_its_fall_through` -- the walk defect that dropped one of the
    three vtable references and 44,713 instruction starts;
  * `test_unreached_site_is_reported_not_confirmed` -- a misaligned decode must not be
    promoted to a confirmed site.

Run:  python -X utf8 -m unittest tests.test_enumerate_accesses
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
    "enumerate_accesses", ROOT / "scripts" / "enumerate-accesses.py")
ea = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(ea)

PIO = ea.PIO_FREE_VA
VT = ea.VTABLE_BASE_VA

HAVE_IMAGE = ((ROOT / "game" / "default.xbe").is_file()
              and (ROOT / "game" / "mygame_analysis.json").is_file()
              and (ROOT / "src" / "recomp" / "gen" / "recomp_dispatch.c").is_file())


def _md():
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    return md


# ------------------------------------------------------------------ synthetic image fixture


class FixtureXbe:
    """A tiny synthetic XBE with one executable section.

    `raw_addr` is 0 so file offsets equal section offsets, which makes the hand-written
    encodings below readable.
    """

    def __init__(self, code: bytes, va: int = 0x00100000):
        self.code = code
        self.va = va

    def __enter__(self):
        self._dir = tempfile.TemporaryDirectory()
        d = Path(self._dir.name)
        self.xbe = d / "default.xbe"
        self.analysis = d / "mygame_analysis.json"
        self.dispatch = d / "recomp_dispatch.c"
        self.xbe.write_bytes(self.code)
        self.analysis.write_text(json.dumps({
            "entry_point": f"0x{self.va:08X}",
            "sections": [{
                "name": ".text",
                "virtual_addr": f"0x{self.va:08X}",
                "raw_addr": "0x00000000",
                "raw_size": len(self.code),
                "executable": True,
            }],
        }), encoding="utf-8")
        self.dispatch.write_text("static const recomp_entry_t g[] = {\n"
                                 f"    {{ 0x{self.va:08X}u, sub_00000000 }},\n"
                                 "};\n", encoding="utf-8")
        return self

    def __exit__(self, *exc):
        self._dir.cleanup()
        return False

    def image(self):
        return ea.XbeImage.load(self.xbe, self.analysis)


def a1_moffs(value: int, reg: str = "eax") -> bytes:
    """`A1`/`A3` + disp32.  Always `eax`, exactly as the lifter's moffs form is."""
    assert reg == "eax"
    return b"\xA1" + struct.pack("<I", value & 0xFFFFFFFF)


def a3_moffs(value: int) -> bytes:
    return b"\xA3" + struct.pack("<I", value & 0xFFFFFFFF)


def modrm_disp32(value: int, reg_field: int = 2) -> bytes:
    """`8B /r` with mod=00, r/m=101: `mov r32, [disp32]`.  Emitted as signed decimal."""
    modrm = (0b00 << 6) | ((reg_field & 7) << 3) | 0b101
    return b"\x8B" + bytes([modrm]) + struct.pack("<I", value & 0xFFFFFFFF)


def ret() -> bytes:
    return b"\xC3"


# ------------------------------------------------------------------------ normalisation


class NormalisationTests(unittest.TestCase):
    def test_signed_displacement_normalises_to_uint32(self):
        """`-25034736` and `0xFE820010` are the same guest VA.  This is the whole point."""
        self.assertEqual(ea.u32(-25034736), PIO)
        self.assertEqual(ea.u32(-25034736), 0xFE820010)
        self.assertEqual(ea.hex32(-25034736), "0xFE820010")

    def test_capstone_reports_the_displacement_as_a_signed_int(self):
        """The signedness is capstone's, not an assumption: prove it from a real decode."""
        code = modrm_disp32(PIO)
        insns = list(_md().disasm(code, 0x1000))
        disp = insns[0].operands[1].mem.disp
        self.assertLess(disp, 0, "capstone must hand back the signed displacement")
        self.assertEqual(ea.u32(disp), PIO)

    def test_every_reported_address_is_masked_hex(self):
        with FixtureXbe(a1_moffs(PIO) + ret()) as fx:
            image = fx.image()
            rep = ea.enumerate_value(image, ea.walk_program(image, [fx.va], _md()), PIO)
            self.assertTrue(rep["sites"])
            for site in rep["sites"]:
                for key in ("instruction_va", "operand_va"):
                    self.assertRegex(site[key], r"^0x[0-9A-F]{8}$")

    def test_high_half_and_low_half_addresses_both_round_trip(self):
        for value in (0xFE820010, 0x80000000, 0x001C4064, 0x00000000, 0xFFFFFFFF):
            self.assertEqual(ea.u32(value), value)
            self.assertEqual(int(ea.hex32(value), 16), value)


# --------------------------------------------------------------------------- operand forms


class OperandFormTests(unittest.TestCase):
    def _sites(self, code, value, entry_off=0):
        """Enumerate `value` over a fixture, reached from the fixture entry point."""
        with FixtureXbe(code) as fx:
            image = fx.image()
            walk = ea.walk_program(image, [fx.va + entry_off], _md())
            return ea.enumerate_value(image, walk, value)

    def test_a1_moffs_absolute_is_a_site(self):
        rep = self._sites(a1_moffs(PIO) + ret(), PIO)
        self.assertEqual(rep["site_count"], 1)
        self.assertEqual(rep["sites"][0]["form"], ea.FORM_MEM_ABSOLUTE)
        self.assertEqual(rep["sites"][0]["encoding"], ea.ENC_A1_MOFFS)

    def test_a3_moffs_absolute_is_a_site(self):
        rep = self._sites(a3_moffs(PIO) + ret(), PIO)
        self.assertEqual(rep["site_count"], 1)
        self.assertEqual(rep["sites"][0]["encoding"], ea.ENC_A3_MOFFS)

    def test_modrm_disp32_absolute_is_a_site(self):
        rep = self._sites(modrm_disp32(PIO) + ret(), PIO)
        self.assertEqual(rep["site_count"], 1)
        self.assertEqual(rep["sites"][0]["encoding"], ea.ENC_MODRM)
        self.assertEqual(rep["sites"][0]["form"], ea.FORM_MEM_ABSOLUTE)

    def test_immediate_operand_is_a_site(self):
        """`mov dword ptr [esi], imm32` -- a constructor storing a vtable base."""
        # C7 06 <imm32>
        code = b"\xC7\x06" + struct.pack("<I", VT) + ret()
        rep = self._sites(code, VT)
        self.assertEqual(rep["site_count"], 1)
        self.assertEqual(rep["sites"][0]["form"], ea.FORM_IMMEDIATE)
        self.assertIn("mov", rep["sites"][0]["text"])

    def test_push_immediate_is_a_site(self):
        # 68 <imm32>
        code = b"\x68" + struct.pack("<I", 0x001C4064) + ret()
        rep = self._sites(code, 0x001C4064)
        self.assertEqual(rep["site_count"], 1)
        self.assertEqual(rep["sites"][0]["form"], ea.FORM_IMMEDIATE)

    def test_disp8_with_base_is_a_displacement_not_a_site(self):
        """`[ebx+0x10]` is an OFFSET.  It must be reported, never counted as a site.

        A `disp8` is only one byte, so the 4-byte raw scan cannot see it at all; this is
        exactly the operand form the recursive-descent walk exists to cover.  Counting
        displacements as sites is what turns a 28-site answer into a garbage one: a small
        offset such as 4 or 0x10 collides with every ordinary struct field access.
        """
        code = b"\x8B\x43\x10" + ret()          # mov eax, [ebx+0x10]
        rep = self._sites(code, 0x10)
        self.assertEqual(rep["site_count"], 0)
        self.assertEqual(rep["raw_occurrences"], 0, "a disp8 is invisible to a dword scan")
        self.assertEqual(rep["displacement_count"], 1)
        d = rep["displacement_matches"][0]
        self.assertEqual(d["base"], "ebx")
        self.assertEqual(d["form"], ea.FORM_MEM_DISPLACEMENT)
        self.assertEqual(d["width_bytes"], 1)

    def test_disp32_with_base_and_index_is_a_displacement_not_a_site(self):
        """`[eax+ecx*4+0x40]` -- base + index + scale + disp32."""
        # 8B 84 88 40 00 00 00  -> mov eax, [eax+ecx*4+0x40]
        code = b"\x8B\x84\x88" + struct.pack("<I", 0x40) + ret()
        rep = self._sites(code, 0x40)
        self.assertEqual(rep["site_count"], 0)
        self.assertEqual(rep["displacement_count"], 1)
        d = rep["displacement_matches"][0]
        self.assertEqual((d["base"], d["index"], d["scale"]), ("eax", "ecx", 4))
        self.assertEqual(d["width_bytes"], 4)

    def test_base_relative_disp32_is_not_confused_with_an_absolute(self):
        """A 0xFE820010 displacement on a register base is NOT the PIO_FREE address.

        This is the sharpest form of the trap: same bytes, same normalised value, different
        meaning.  Only a `mem-absolute` operand is an address reference.
        """
        # 8B 83 <disp32>  -> mov eax, [ebx+0xFE820010]
        code = b"\x8B\x83" + struct.pack("<I", PIO) + ret()
        rep = self._sites(code, PIO)
        self.assertEqual(rep["site_count"], 0)
        self.assertEqual(rep["raw_occurrences"], 1, "the dword is there, but it is an offset")
        self.assertEqual(rep["displacement_count"], 1)

    def test_branch_target_is_code_not_an_access(self):
        """`call 0x0014CF20` references code, not a data location.

        Conflating the two would report every call target as a guest access and make the
        vtable-base count meaningless.
        """
        call_at, target = 0x00, 0x20
        code = bytearray(b"\x90" * 0x21)
        code[0:5] = b"\xE8" + struct.pack("<i", target - (call_at + 5))
        code[0x20] = 0xC3
        with FixtureXbe(bytes(code)) as fx:
            image = fx.image()
            walk = ea.walk_program(image, [fx.va], _md())
            rep = ea.enumerate_value(image, walk, fx.va + target)
            self.assertEqual(rep["site_count"], 0)
            self.assertEqual(rep["code_target_count"], 1)
            self.assertEqual(rep["code_targets"][0]["form"], ea.FORM_CODE_TARGET)


# --------------------------------------------------------------------------- enumeration


class RawScanTests(unittest.TestCase):
    def _rep(self, code, value, entry_off=0):
        with FixtureXbe(code) as fx:
            image = fx.image()
            walk = ea.walk_program(image, [fx.va + entry_off], _md())
            return image, ea.enumerate_value(image, walk, value)

    def test_unaligned_immediate_is_found(self):
        """`mov dword ptr [esi], imm32` puts the value at a non-4-aligned address.

        `docs/jsrf-technical-record.md` section 6: "Aligned dword scans miss unaligned
        immediates (`mov dword ptr [esi], imm32` put `0x001E1270` at a non-4-aligned
        address); raw-byte scans are the fallback."
        """
        # one NOP, then C7 06 <imm32> at offset 1, so the immediate lands at offset 3
        code = b"\x90" + b"\xC7\x06" + struct.pack("<I", VT) + ret()
        with FixtureXbe(code) as fx:
            image = fx.image()
            hits = image.raw_occurrences(VT)
            self.assertEqual(len(hits), 1)
            self.assertNotEqual(hits[0][0] % 4, 0, "the immediate must be unaligned")
            # an aligned-only scan would miss it entirely
            aligned = [va for va, _ in hits if va % 4 == 0]
            self.assertEqual(aligned, [])
            walk = ea.walk_program(image, [fx.va], _md())
            rep = ea.enumerate_value(image, walk, VT)
            self.assertEqual(rep["site_count"], 1)
            self.assertEqual(rep["sites"][0]["form"], ea.FORM_IMMEDIATE)

    def test_negative_control_finds_nothing(self):
        _, rep = self._rep(a1_moffs(PIO) + ret(), 0xFE820011)
        self.assertEqual(rep["site_count"], 0)
        self.assertEqual(rep["raw_occurrences"], 0)

    def test_duplicate_sites_are_all_reported(self):
        """Two instructions reading the same address are two sites, at two starts.

        The `jmp` between them is what keeps both reachable: a `ret` would end the walk and
        make the second site UNREACHED, which is the tool's `test_unreached_site_is_reported`
        behaviour, not this one.
        """
        code = (a1_moffs(PIO) + b"\xEB\x00"          # jmp +0 -> fall through to va+7
                + modrm_disp32(PIO) + ret())
        _, rep = self._rep(code, PIO)
        self.assertEqual(rep["site_count"], 2)
        vas = [r["instruction_va"] for r in rep["sites"]]
        self.assertEqual(len(set(vas)), 2)
        self.assertEqual(rep["unreached_count"], 0)

    def test_data_equal_to_the_value_is_unlocated_not_a_site(self):
        """A dword that is not the trailing operand of any instruction is data."""
        code = struct.pack("<I", PIO) + ret()
        _, rep = self._rep(code, PIO, entry_off=4)
        self.assertEqual(rep["site_count"], 0)
        self.assertEqual(rep["raw_occurrences"], 1)
        self.assertEqual(rep["unlocated_count"], 1)

    def test_coincidental_decode_is_data_not_a_site(self):
        """x86 is not self-synchronising: one operand dword can end several decodable
        instructions.  Only the one that references the value *by operand* is a site.

        `90 C7 06 <imm32>` at va+0 decodes as `nop; mov [esi], imm32` -- a real site at
        va+1.  The same bytes also decode as an `adc eax, imm32` from va+0, which references
        the value too, so the operand dword has two interpretations.  The site count must
        stay 1: an interpretation is not a site.
        """
        code = b"\x90" + b"\xC7\x06" + struct.pack("<I", VT) + ret()
        image, rep = self._rep(code, VT)
        self.assertEqual(rep["site_count"], 1)
        self.assertEqual(rep["sites"][0]["instruction_va"], ea.hex32(0x00100001))
        self.assertEqual(rep["sites"][0]["form"], ea.FORM_IMMEDIATE)
        self.assertEqual(rep["raw_occurrences"], 1)
        self.assertLessEqual(rep["ambiguous_count"], 1)

    def test_mid_instruction_start_is_not_reported_as_a_site(self):
        """The recorded hazard: `operand_va - 2` lands inside an instruction.

        `33 C0` (xor eax,eax) then `A1 <disp32>` at va+2 puts the operand at va+3.  A
        caller that assumed the ModRM convention would start at va+1 -- the second byte of
        the `xor` -- and capstone would render plausible garbage.  The tool must resolve the
        start by decoding and must not report va+1.
        """
        code = b"\x33\xC0" + a1_moffs(PIO) + ret()
        with FixtureXbe(code) as fx:
            image = fx.image()
            located = ea.locate_instructions(image, fx.va + 3, _md())
            starts = [c["start"] for c in located]
            self.assertIn(fx.va + 2, starts)
            self.assertNotIn(fx.va + 1, starts)


# ------------------------------------------------------------------------ recursive descent


class WalkTests(unittest.TestCase):
    def _walk(self, code):
        with FixtureXbe(code) as fx:
            return fx, ea.walk_program(fx.image(), [fx.va], _md())

    def test_fall_through_is_followed(self):
        with FixtureXbe(a1_moffs(PIO) + ret()) as fx:
            walk = ea.walk_program(fx.image(), [fx.va], _md())
            self.assertIn(fx.va, walk)
            self.assertIn(fx.va + 5, walk)

    def test_direct_call_target_and_fall_through_are_both_followed(self):
        # va+0: E8 <rel32> call va+0x20 ; va+5: C3 ret ; ... va+0x20: C3 ret
        call_at, target = 0x00, 0x20
        rel = target - (call_at + 5)
        code = bytearray(b"\x90" * 0x21)
        code[0:5] = b"\xE8" + struct.pack("<i", rel)
        code[5] = 0xC3
        code[0x20] = 0xC3
        with FixtureXbe(bytes(code)) as fx:
            walk = ea.walk_program(fx.image(), [fx.va], _md())
            self.assertIn(fx.va + target, walk, "call target")
            self.assertIn(fx.va + 5, walk, "call fall-through")

    def test_indirect_call_keeps_its_fall_through(self):
        """`call [mem]` returns, so the next instruction IS reachable.

        Dropping this fall-through truncated the real walk by 44,713 instruction starts and
        lost one of the three vtable-base references.  The synthetic form here is the same
        shape: `FF 15 <disp32>` (call [disp32]) then an `A1` load of the queried value.
        """
        # va+0: FF 15 <disp32>  call dword ptr [0x00100040]
        # va+6: A1 <PIO>        mov eax, [PIO_FREE]
        # va+11: C3             ret
        code = bytearray(b"\x90" * 0x40)
        code[0:6] = b"\xFF\x15" + struct.pack("<I", 0x00100040)
        code[6:11] = a1_moffs(PIO)
        code[11] = 0xC3
        with FixtureXbe(bytes(code)) as fx:
            image = fx.image()
            walk = ea.walk_program(image, [fx.va], _md())
            self.assertIn(fx.va + 6, walk, "fall-through after an indirect call")
            rep = ea.enumerate_value(image, walk, PIO)
            self.assertEqual(rep["site_count"], 1)
            self.assertEqual(rep["unreached_count"], 0)

    def test_indirect_jump_does_not_invent_a_successor(self):
        # FF 25 <disp32>  jmp dword ptr [0x00100040]
        code = bytearray(b"\x90" * 0x40)
        code[0:6] = b"\xFF\x25" + struct.pack("<I", 0x00100040)
        code[6:11] = a1_moffs(PIO)          # unreachable: jmp does not fall through
        code[11] = 0xC3
        with FixtureXbe(bytes(code)) as fx:
            walk = ea.walk_program(fx.image(), [fx.va], _md())
            self.assertNotIn(fx.va + 6, walk)

    def test_conditional_branch_follows_both_edges(self):
        # 74 05  je +5   ->  target va+7, fall-through va+2
        code = bytearray(b"\x90" * 0x20)
        code[0:2] = b"\x74\x05"
        code[2] = 0xC3
        code[7] = 0xC3
        with FixtureXbe(bytes(code)) as fx:
            walk = ea.walk_program(fx.image(), [fx.va], _md())
            self.assertIn(fx.va + 2, walk)
            self.assertIn(fx.va + 7, walk)

    def test_ret_does_not_fall_through(self):
        code = b"\xC3" + a1_moffs(PIO) + b"\xC3"
        with FixtureXbe(code) as fx:
            walk = ea.walk_program(fx.image(), [fx.va], _md())
            self.assertNotIn(fx.va + 1, walk)

    def test_unreached_site_is_reported_not_confirmed(self):
        """The control that stops a misaligned decode becoming a site.

        `ret` at va+0 makes va+1 unreachable, so the `A1` PIO load at va+1 is a real
        instruction the walk never reached.  It must land in `unreached`, not in `sites`.
        """
        code = b"\xC3" + a1_moffs(PIO) + b"\xC3"
        with FixtureXbe(code) as fx:
            image = fx.image()
            walk = ea.walk_program(image, [fx.va], _md())
            rep = ea.enumerate_value(image, walk, PIO)
            self.assertEqual(rep["site_count"], 0, "unreached must not be confirmed")
            self.assertEqual(rep["unreached_count"], 1)
            self.assertFalse(rep["unreached"][0]["reached"])

    def test_visited_set_terminates_a_loop(self):
        # EB FE  jmp $  (self-loop)
        with FixtureXbe(b"\xEB\xFE") as fx:
            walk = ea.walk_program(fx.image(), [fx.va], _md())
            self.assertEqual(len(walk.starts), 1)

    def test_seed_outside_any_section_is_ignored(self):
        with FixtureXbe(a1_moffs(PIO) + ret()) as fx:
            walk = ea.walk_program(fx.image(), [fx.va, 0x7F000000], _md())
            self.assertIn(fx.va, walk)


# --------------------------------------------------------------------------- reconciliation


class GeneratedSpellingTests(unittest.TestCase):
    def _gen(self, body: str) -> ea.Path:
        d = tempfile.TemporaryDirectory()
        self.addCleanup(d.cleanup)
        gen = Path(d.name) / "gen"
        gen.mkdir()
        (gen / "chunk.c").write_text(body, encoding="utf-8")
        return gen

    def test_both_spellings_are_found_by_value(self):
        gen = self._gen("eax = MEM32(0xFE820010u);\n"
                        "edx = MEM32(-25034736);\n")
        res = ea.scan_generated(gen, PIO)
        self.assertEqual(res["total"], 2)
        self.assertEqual(res["by_spelling"], {"decimal": 1, "hex": 1})

    def test_hex_spelling_only_would_undercount(self):
        """The recorded failure: a one-spelling grep found 10 of 28 sites.

        Here the population is exactly the real split -- 10 hex and 18 signed decimal --
        and the test asserts that the hex-only view reports 10, which is the wrong answer.
        """
        hex_lines = "".join(f"eax = MEM32(0x{PIO:08X}u);\n" for _ in range(10))
        dec_lines = "".join(f"edx = MEM32({PIO - 0x100000000});\n" for _ in range(18))
        gen = self._gen(hex_lines + dec_lines)
        res = ea.scan_generated(gen, PIO)
        self.assertEqual(res["total"], 28)
        self.assertEqual(res["by_spelling"], {"decimal": 18, "hex": 10})
        self.assertEqual(res["by_spelling"]["hex"], 10,
                         "the hex-only spelling view is the known-bad undercount")

    def test_other_mem_widths_are_matched(self):
        gen = self._gen("a = MEM8(0xFE820010u); b = MEM16(-25034736); c = MEM32(0xFE820010);\n")
        self.assertEqual(ea.scan_generated(gen, PIO)["total"], 3)

    def test_non_matching_values_are_excluded(self):
        gen = self._gen("a = MEM32(0xFE820011u); b = MEM32(-25034735);\n")
        self.assertEqual(ea.scan_generated(gen, PIO)["total"], 0)

    def test_missing_directory_is_rejected(self):
        with self.assertRaises(ea.EnumerateError):
            ea.scan_generated(Path("does/not/exist"), PIO)

    def test_directory_without_c_files_is_rejected(self):
        d = tempfile.TemporaryDirectory()
        self.addCleanup(d.cleanup)
        with self.assertRaises(ea.EnumerateError):
            ea.scan_generated(Path(d.name), PIO)


class DispatchSeedTests(unittest.TestCase):
    def test_dispatch_entries_are_parsed(self):
        d = tempfile.TemporaryDirectory()
        self.addCleanup(d.cleanup)
        p = Path(d.name) / "recomp_dispatch.c"
        p.write_text("static const recomp_entry_t g_recomp_dispatch[] = {\n"
                     "    { 0x0014CF20u, sub_0014CF20 },\n"
                     "    { 0x0014CF00u, sub_0014CF00 },\n"
                     "};\n", encoding="utf-8")
        self.assertEqual(ea.dispatch_seeds(p), [0x0014CF00, 0x0014CF20])

    def test_missing_dispatch_is_rejected(self):
        with self.assertRaises(ea.EnumerateError):
            ea.dispatch_seeds(Path("nope.c"))


class TranspositionTests(unittest.TestCase):
    def test_interior_byte_is_not_a_walked_instruction_start(self):
        """The recorded historical error: `0x00193D62` written for `0x00193D96`.

        In the real XBE `0x00193D61` is a 6-byte instruction
        (`mov [ebx+0x40071c], eax`), so `0x00193D62` is its second byte -- interior, never
        reached as a start.  `0x00193D96` is `mov esi, ecx`, a real start.  The fixture
        reproduces that shape.

        Note that a bare decode discriminates nothing here: `0x00193D62` decodes to
        something from almost any byte.  The predicate that separates them is whether the
        recursive descent visited the address as an instruction start.
        """
        code = bytearray(b"\x90" * 0x60)
        # va+0x11: 89 83 1C 07 40 00   mov [ebx+0x40071c], eax   (6 bytes)
        code[0x11:0x17] = b"\x89\x83" + struct.pack("<I", 0x0040071C)
        # va+0x46: 8B F1               mov esi, ecx            (2 bytes)
        code[0x46:0x48] = b"\x8B\xF1"
        code[0x48] = 0xC3
        with FixtureXbe(bytes(code)) as fx:
            image = fx.image()
            walk = ea.walk_program(image, [fx.va], _md())
            left, right = fx.va + 0x12, fx.va + 0x46      # the 0x00193D62 / 0x00193D96 shape
            self.assertIn(fx.va + 0x11, walk)
            self.assertNotIn(left, walk, "interior byte must not be a start")
            self.assertIn(right, walk)
            self.assertFalse(
                ea.boundary_evidence(image, left, walk, _md())["reached_as_instruction_start"])
            right_ev = ea.boundary_evidence(image, right, walk, _md())
            self.assertTrue(right_ev["reached_as_instruction_start"])
            self.assertEqual(right_ev["text"], "mov esi, ecx")

    def test_boundary_evidence_accepts_a_start_with_no_operand_dword(self):
        """`mov esi, ecx` has no trailing dword, so `locate_instructions` finds nothing.

        This is why the boundary probe must not be built on `locate_instructions`: doing so
        answers "not a boundary" for most real instructions.
        """
        code = b"\x8B\xF1" + ret()
        with FixtureXbe(code) as fx:
            image = fx.image()
            walk = ea.walk_program(image, [fx.va], _md())
            self.assertEqual(ea.locate_instructions(image, fx.va, _md()), [])
            self.assertTrue(
                ea.boundary_evidence(image, fx.va, walk, _md())["reached_as_instruction_start"])

    def test_locate_instructions_never_returns_an_interior_start(self):
        """Whatever `locate_instructions` returns must decode from an exact boundary."""
        code = b"\x90" + b"\xC7\x06" + struct.pack("<I", VT) + ret()
        with FixtureXbe(code) as fx:
            image = fx.image()
            for operand_va, _ in image.raw_occurrences(VT):
                for cand in ea.locate_instructions(image, operand_va, _md()):
                    self.assertEqual(cand["start"] + cand["length"], operand_va + 4)


# ------------------------------------------------------------------------ the real XBE

@unittest.skipUnless(HAVE_IMAGE, "original XBE, analysis and dispatch table required")
class RealImageTests(unittest.TestCase):
    """Plan T9's PASS criteria, measured on the original XBE.

    These are the acceptance tests.  They assert the recorded values -- 28 sites
    (10 hex + 18 decimal) and 3 vtable references -- and that every control passes.
    """

    @classmethod
    def setUpClass(cls):
        cls.report, cls.controls = ea.analyse(
            ROOT / "game" / "default.xbe",
            ROOT / "game" / "mygame_analysis.json",
            ROOT / "src" / "recomp" / "gen",
            ROOT / "src" / "recomp" / "gen" / "recomp_dispatch.c",
            [PIO, VT, ea.NEGATIVE_CONTROL_VA,
             ea.TRANSPOSITION_LEFT, ea.TRANSPOSITION_RIGHT])

    def test_all_controls_pass(self):
        failed = [c.line() for c in self.controls if not c.passed]
        self.assertEqual(failed, [], "failing controls:\n" + "\n".join(failed))

    def test_pio_free_site_count_is_28(self):
        rep = self.report["queries"][ea.hex32(PIO)]
        self.assertEqual(rep["site_count"], ea.EXPECT_PIO_SITES)

    def test_pio_free_split_is_10_hex_and_18_decimal(self):
        gen = self.report["generated_pio_free"]
        self.assertEqual(gen["by_spelling"].get("hex"), ea.EXPECT_PIO_HEX)
        self.assertEqual(gen["by_spelling"].get("decimal"), ea.EXPECT_PIO_DECIMAL)
        self.assertEqual(gen["total"], ea.EXPECT_PIO_SITES)

    def test_xbe_encoding_split_matches_the_generated_spelling_split(self):
        """The mechanism, not a coincidence.

        The lifter emits an `A1` moffs operand as hex and a ModRM `disp32` operand as signed
        decimal.  The XBE's own encoding histogram must therefore equal the generated
        spelling histogram: 10 and 18.
        """
        rep = self.report["queries"][ea.hex32(PIO)]
        gen = self.report["generated_pio_free"]
        self.assertEqual(rep["sites_by_encoding"].get(ea.ENC_A1_MOFFS),
                         gen["by_spelling"].get("hex"))
        self.assertEqual(rep["sites_by_encoding"].get(ea.ENC_MODRM),
                         gen["by_spelling"].get("decimal"))

    def test_vtable_base_has_three_references(self):
        rep = self.report["queries"][ea.hex32(VT)]
        self.assertEqual(rep["site_count"], ea.EXPECT_VTABLE_REFS)
        self.assertEqual(rep["unreached_count"], 0)

    def test_negative_control_is_zero(self):
        rep = self.report["queries"][ea.hex32(ea.NEGATIVE_CONTROL_VA)]
        self.assertEqual(rep["site_count"], 0)
        self.assertEqual(rep["raw_occurrences"], 0)

    def test_transposition_control_distinguishes_the_two_addresses(self):
        t = self.report["transposition"]
        self.assertTrue(t["distinguishable"])
        self.assertFalse(t["left_evidence"]["reached_as_instruction_start"])
        self.assertTrue(t["right_evidence"]["reached_as_instruction_start"])
        self.assertEqual(t["right_evidence"]["text"], "mov esi, ecx")
        # the interior address must be legible: name the instruction it sits inside
        self.assertIsNotNone(t["left_evidence"]["contained_in"])
        self.assertEqual(t["left_evidence"]["contained_in"]["instruction_va"],
                         "0x00193D61")
        self.assertEqual(t["left_evidence"]["contained_in"]["length"], 6)

    def test_no_site_is_silently_dropped(self):
        """Every raw operand dword is accounted for as a site, an unreached site, a
        displacement, a branch target, or data.  Nothing vanishes between the alignment-
        independent scan and the report."""
        for value in (PIO, VT):
            rep = self.report["queries"][ea.hex32(value)]
            accounted = (rep["site_count"] + rep["unreached_count"]
                         + rep["unlocated_count"])
            self.assertEqual(rep["raw_occurrences"], accounted,
                             f"{ea.hex32(value)}: raw {rep['raw_occurrences']} != "
                             f"accounted {accounted} "
                             f"(sites {rep['site_count']}, unreached {rep['unreached_count']}, "
                             f"unlocated {rep['unlocated_count']})")

    def test_walk_actually_ran_over_the_program(self):
        walk = self.report["walk"]
        self.assertGreater(walk["instruction_starts_reached"], 100_000)
        self.assertFalse(walk["exhausted"])

    def test_report_declares_what_it_cannot_see(self):
        cannot = self.report["method"]["cannot_see"]
        self.assertTrue(cannot)
        joined = " ".join(cannot).lower()
        for phrase in ("register-indirect", "computed", "table-driven"):
            self.assertIn(phrase, joined)

    def test_repeat_runs_are_identical(self):
        again, _ = ea.analyse(
            ROOT / "game" / "default.xbe",
            ROOT / "game" / "mygame_analysis.json",
            ROOT / "src" / "recomp" / "gen",
            ROOT / "src" / "recomp" / "gen" / "recomp_dispatch.c",
            [PIO, VT, ea.NEGATIVE_CONTROL_VA,
             ea.TRANSPOSITION_LEFT, ea.TRANSPOSITION_RIGHT])
        self.assertEqual(json.dumps(again, sort_keys=True),
                         json.dumps(self.report, sort_keys=True))


class ControlBehaviourTests(unittest.TestCase):
    """The plan requires each run to print its own controls and exit nonzero if one fails.

    A control that cannot fail is not a control, so these tests make a control fail on
    purpose and assert that the failure is both detected and reported through the exit code.
    """

    def test_wrong_site_expectation_makes_a_control_fail(self):
        original = ea.EXPECT_PIO_SITES
        self.addCleanup(setattr, ea, "EXPECT_PIO_SITES", original)
        ea.EXPECT_PIO_SITES = 27
        pio = {"site_count": 28, "raw_occurrences": 28, "unreached_count": 0,
               "unlocated_count": 0, "sites_by_encoding": {ea.ENC_A1_MOFFS: 10,
                                                           ea.ENC_MODRM: 18},
               "value": "0xFE820010"}
        vtable = {"site_count": 3, "raw_occurrences": 3, "unreached_count": 0,
                  "value": "0x001E0F00"}
        negative = {"site_count": 0, "raw_occurrences": 0, "unreached_count": 0,
                    "value": "0xFE820011"}
        gen = {"total": 28, "by_spelling": {"hex": 10, "decimal": 18}}
        boundaries = {
            "left": {"address": "0x00193D62", "reached_as_instruction_start": False,
                     "contained_in": None},
            "right": {"address": "0x00193D96", "reached_as_instruction_start": True,
                      "text": "mov esi, ecx"},
        }
        controls = ea.run_controls(pio, vtable, negative, gen, boundaries)
        failed = [c for c in controls if not c.passed]
        self.assertTrue(failed, "a wrong expectation must fail a control")
        self.assertTrue(any("site count" in c.name for c in failed))

    def test_indistinguishable_transposition_makes_a_control_fail(self):
        pio = {"site_count": 28, "raw_occurrences": 28, "unreached_count": 0,
               "unlocated_count": 0, "sites_by_encoding": {ea.ENC_A1_MOFFS: 10,
                                                           ea.ENC_MODRM: 18},
               "value": "0xFE820010"}
        vtable = {"site_count": 3, "raw_occurrences": 3, "unreached_count": 0,
                  "value": "0x001E0F00"}
        negative = {"site_count": 0, "raw_occurrences": 0, "unreached_count": 0,
                    "value": "0xFE820011"}
        gen = {"total": 28, "by_spelling": {"hex": 10, "decimal": 18}}
        # both sides identical: the control cannot discriminate
        boundaries = {
            "left": {"address": "0x00193D96", "reached_as_instruction_start": True,
                     "contained_in": None},
            "right": {"address": "0x00193D96", "reached_as_instruction_start": True,
                      "text": "mov esi, ecx"},
        }
        controls = ea.run_controls(pio, vtable, negative, gen, boundaries)
        failed = [c for c in controls if not c.passed]
        self.assertTrue(any("transposition" in c.name for c in failed))

    @unittest.skipUnless(HAVE_IMAGE, "original XBE, analysis and dispatch table required")
    def test_main_exits_nonzero_when_a_control_fails(self):
        """The exit code is the contract: a failed control must be visible to a caller."""
        original = ea.EXPECT_PIO_SITES
        self.addCleanup(setattr, ea, "EXPECT_PIO_SITES", original)
        ea.EXPECT_PIO_SITES = 27
        rc = ea.main([])
        self.assertEqual(rc, 1, "a failing control must exit nonzero")

    @unittest.skipUnless(HAVE_IMAGE, "original XBE, analysis and dispatch table required")
    def test_main_exits_zero_when_controls_pass(self):
        self.assertEqual(ea.main([]), 0)


class ReportShapeTests(unittest.TestCase):
    def test_control_line_marks_pass_and_fail(self):
        self.assertIn("[PASS]", ea.Control("x", True, "d").line())
        self.assertIn("[FAIL]", ea.Control("x", False, "d").line())

    def test_cannot_see_text_names_all_three_blind_spots(self):
        joined = " ".join(ea.CANNOT_SEE).lower()
        self.assertIn("register-indirect", joined)
        self.assertIn("computed", joined)
        self.assertIn("table-driven", joined)
        self.assertIn("vtable", joined)

    def test_expected_constants_are_the_recorded_values(self):
        """Guard the recorded expectations against a silent edit."""
        self.assertEqual(ea.EXPECT_PIO_SITES, 28)
        self.assertEqual(ea.EXPECT_PIO_HEX, 10)
        self.assertEqual(ea.EXPECT_PIO_DECIMAL, 18)
        self.assertEqual(ea.EXPECT_VTABLE_REFS, 3)
        self.assertEqual(ea.PIO_FREE_VA, 0xFE820010)
        self.assertEqual(ea.VTABLE_BASE_VA, 0x001E0F00)
        self.assertEqual(ea.u32(-25034736), ea.PIO_FREE_VA)


if __name__ == "__main__":
    unittest.main()
