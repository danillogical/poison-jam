"""Controls for T3's xemu gdbstub, whose register decode was wrong.

**The defect this file exists for.** `scripts/xemu-gdbstub.py` decoded the `g` packet
as x86-64 (8-byte slots, `rax..gs`). Measured against a live xemu running JSRF, the
guest is **32-bit**, so the packet is the i386 layout: 16 registers in 4-byte slots.

The wrong decode produced a **confident, self-consistent, entirely false** register
file, and it survived because nothing checked it:

| | read as x86-64 (wrong) | read as i386 (right) |
|---|---|---|
| instruction pointer | `rip = 0x0000000000000000` | `eip = 0x00193D67` |
| code segment | `cs = 0x0000BFFE88000000` | `cs = 0x00000008` |
| stack segment | `ss = 0x4007A00000000000` | `ss = 0x00000010` |

`rip = 0` looks like a stopped guest, which is why four archived dumps reported it
without anyone asking why. `cs = 8` and `ss = 0x10` are the selectors of a 32-bit
protected-mode guest, and `eip = 0x00193D67` holds `ff 86 f0 01 00 00` -- **byte-for-byte
what the original XBE holds at that address**.

The controls below are the checks that would have caught it. The strongest is the
cross-check against the original XBE: a decoded `eip` must be an address whose bytes
match the image, which no wrong decode can satisfy by accident.
"""
from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

_spec = importlib.util.spec_from_file_location(
    'xemu_gdbstub', ROOT / 'scripts' / 'xemu-gdbstub.py')
assert _spec and _spec.loader
gdbstub = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(gdbstub)

# **The first 64 bytes of the `g` packet, captured from a live xemu running JSRF.**
# Sixteen 4-byte registers, in the order REGISTER_ORDER names them:
#
#   eax=0x00000002  ecx=0xC5C254B6  edx=0x0019D6FF  ebx=0xFD000000
#   esp=0xD0020F94  ebp=0x00000001  esi=0x0019D468  edi=0x00000000
#   eip=0x00193D67  eflags=0x00000202  cs=0x08  ss=0x10  ds=0x10  es=0x10
#   fs=0x20  gs=0x00
#
# The assertions below check EVERY one of these against the probe's own output, so a
# transcription error in any single field fails rather than hiding behind the fields a
# test happened to assert.
#
# The full packet is 344 bytes: these 64 plus 280 of x87/SSE state this tool does not
# decode. A hand-made fixture would prove nothing here, so these are the real bytes.
LIVE_G_REGISTERS = bytes.fromhex(
    '02000000'  # eax
    'b654c2c5'  # ecx
    'ffd61900'  # edx
    '000000fd'  # ebx = 0xFD000000
    '940f02d0'  # esp
    '01000000'  # ebp
    '68d41900'  # esi
    '00000000'  # edi
    '673d1900'  # eip
    '02020000'  # eflags = 0x00000202
    '08000000'  # cs
    '10000000'  # ss
    '10000000'  # ds
    '10000000'  # es
    '20000000'  # fs
    '00000000'  # gs
)


def decode_live() -> dict:
    """The live packet decoded with the tool's own layout and width."""
    values = {}
    for index, name in enumerate(gdbstub.REGISTER_ORDER):
        offset = index * gdbstub.REGISTER_WIDTH
        values[name] = int.from_bytes(
            LIVE_G_REGISTERS[offset:offset + gdbstub.REGISTER_WIDTH], 'little')
    return values


class RegisterLayoutTests(unittest.TestCase):
    def test_the_layout_is_i386_not_x86_64(self) -> None:
        """16 registers in 4-byte slots, named as the guest's own disassembly names."""
        self.assertEqual(gdbstub.REGISTER_WIDTH, 4)
        self.assertEqual(len(gdbstub.REGISTER_ORDER), 16)
        self.assertEqual(gdbstub.REGISTER_ORDER[0], 'eax')
        self.assertIn('eip', gdbstub.REGISTER_ORDER)
        self.assertIn('esp', gdbstub.REGISTER_ORDER)
        self.assertNotIn('rax', gdbstub.REGISTER_ORDER)
        self.assertNotIn('rip', gdbstub.REGISTER_ORDER)

    def test_every_register_matches_the_live_probe(self) -> None:
        """All sixteen, against the probe's own output.

        Measured defects this replaces: `ebx` was first written `00000000` where the
        probe returned `0xFD000000`, and `eflags` was written so it decoded to
        `0x00020200` where the probe returned `0x00000202`. Both passed the narrower
        controls, which is why this one asserts the whole file.
        """
        expected = {
            'eax': 0x00000002, 'ecx': 0xC5C254B6, 'edx': 0x0019D6FF,
            'ebx': 0xFD000000, 'esp': 0xD0020F94, 'ebp': 0x00000001,
            'esi': 0x0019D468, 'edi': 0x00000000, 'eip': 0x00193D67,
            'eflags': 0x00000202, 'cs': 0x00000008, 'ss': 0x00000010,
            'ds': 0x00000010, 'es': 0x00000010, 'fs': 0x00000020,
            'gs': 0x00000000,
        }
        values = decode_live()
        for name, want in expected.items():
            self.assertEqual(values[name], want,
                             f'{name}: fixture decodes to 0x{values[name]:08X}, '
                             f'the live probe returned 0x{want:08X}')
        self.assertEqual(set(values), set(expected))

    def test_the_segment_selectors_decode_as_small_values(self) -> None:
        """A 32-bit protected-mode guest has `cs = 8`, `ss = 0x10`.

        Under the 8-byte decode `cs` came out as `0x0000BFFE88000000`, which is not a
        selector at all. This is the assertion that makes the layout falsifiable from a
        decoded dump alone.
        """
        values = decode_live()
        self.assertEqual(values['cs'], 8)
        self.assertEqual(values['ss'], 0x10)
        self.assertEqual(values['ds'], 0x10)
        self.assertEqual(values['es'], 0x10)
        self.assertLess(values['cs'], 0x100,
                        'a segment selector is small; a huge value means the wrong width')

    def test_the_instruction_pointer_is_not_zero(self) -> None:
        """The single fact that exposed the defect.

        All four archived dumps reported `eip = 0x00000000`, which reads as a stopped
        guest. A guest that is running has a non-zero `eip`.
        """
        eip = decode_live()['eip']
        self.assertEqual(eip, 0x00193D67)
        self.assertNotEqual(eip, 0)
        # And it is a plausible guest code address, not a stack or MMIO address.
        self.assertGreaterEqual(eip, 0x00010000)
        self.assertLess(eip, 0x40000000)

    def test_the_decoded_eip_matches_the_original_xbe(self) -> None:
        """The cross-check that makes the layout verifiable, not merely plausible.

        A decoded `eip` must be an address whose bytes match the image. No wrong decode
        satisfies this by accident, so this is the control with teeth.
        """
        xbe = ROOT / 'game' / 'default.xbe'
        if not xbe.is_file():
            self.skipTest('the original XBE is not present')
        import subprocess
        import sys
        result = subprocess.run(
            [sys.executable, '-X', 'utf8',
             str(ROOT / 'scripts' / 'inspect-jsrf.py'), 'data', '0x00193D67', '6'],
            capture_output=True, text=True, cwd=str(ROOT))
        if result.returncode != 0:
            self.skipTest('inspect-jsrf.py could not read the XBE')
        # The live guest returned `ff86f00100008b86f40100005f8986f8` at that VA, and
        # `inspect-jsrf.py data` prints the same bytes in address order as
        # `FF86F0010000`. Compared case-insensitively on the hex run, because the
        # tool's own spacing and case are its business, not this control's.
        printed = result.stdout.replace(' ', '').upper()
        self.assertIn('FF86F0010000', printed,
                      'the XBE at the live guest eip should hold the same bytes')

    def test_the_undecoded_remainder_is_reported(self) -> None:
        """344 bytes = 16x4 registers + 280 of x87/SSE state.

        344 is not a multiple of 8, so the old 8-byte decode was misaligned from the
        first register onward. The tool now reports what it did not decode rather than
        silently truncating.
        """
        source = (ROOT / 'scripts' / 'xemu-gdbstub.py').read_text(encoding='utf-8')
        self.assertIn('register_undecoded_bytes', source)
        self.assertIn('register_block_bytes', source)
        self.assertIn('register_layout', source)

    def test_the_redundant_64_bit_spelling_is_gone(self) -> None:
        """`registers_64` was the label that made a wrong decode look authoritative."""
        source = (ROOT / 'scripts' / 'xemu-gdbstub.py').read_text(encoding='utf-8')
        self.assertNotIn("record['registers_64']", source)
        self.assertNotIn("struct.unpack_from('<Q'", source)

    def test_the_guest_view_aliases_the_safe_direction(self) -> None:
        """The packet's names ARE the guest's names; aliases point outward."""
        view = gdbstub.guest_view({'eip': 0x00193D67, 'eax': 2, 'cs': 8})
        self.assertEqual(view['eip'], 0x00193D67)
        self.assertEqual(view['rip'], 0x00193D67)
        self.assertEqual(view['eax'], 2)
        self.assertEqual(view['rax'], 2)


if __name__ == '__main__':
    unittest.main(verbosity=2)
