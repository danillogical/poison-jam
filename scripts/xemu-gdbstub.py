"""`scripts/xemu-gdbstub.py`: stop xemu at a guest PC and dump registers/memory.

Plan T3's middle piece. xemu speaks QEMU's GDB remote serial protocol on
`-gdb tcp::PORT`, and this drives it directly rather than requiring a `gdb`
binary: the project's hosts have no `gdb` on `PATH`, and the protocol surface
needed here is four packets.

**Why raw RSP rather than gdb.** Measured: `gdb` is not on `PATH` on the primary
Windows host. A tool that needs a debugger installed to compare against an
emulator is a tool that will not run when the comparison is needed. The packets
used are `?` (stop reason), `g` (all registers), `m` (read memory) and `k`
(terminate); `qSupported` is optional and this build does not answer it before a
stop.

**Register layout.** x86-64 RSP returns 24 registers in this order:
rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp, r8..r15, rip, eflags, cs, ss, ds, es, fs,
gs -- little-endian hex. The **guest** is 32-bit x86 running inside the emulator,
so its state lives in the low 32 bits of those registers, and `eip` is the low
half of `rip`. The dump reports both the 64-bit value and the guest-width
truncation, because conflating them is how a guest address gets compared against
a host one.

**Nothing is written back.** Every packet used is a read or a stop; there is no
`M`, no `P`, no `c`. An oracle that can mutate the guest is not an oracle, and the
T3 comparison must not be able to change what it compares.
"""
from __future__ import annotations

import argparse
import json
import socket
import struct
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# **The guest is 32-bit, so the `g` packet is the i386 layout: 16 registers in
# 4-byte slots.** Measured against a live xemu running JSRF, and the earlier
# assumption was wrong in a way that produced confident nonsense:
#
#   read as i386 (4-byte):   eip = 0x00193D67   cs = 0x08  ss = 0x10  ds = 0x10
#   read as x86-64 (8-byte): rip = 0x0000000000000000  cs = 0x0000BFFE88000000
#
# `cs = 8`, `ss = 0x10`, `ds = 0x10` are the segment selectors of a 32-bit
# protected-mode guest, and `eip = 0x00193D67` holds `ff 86 f0 01 00 00` -- which is
# **byte-for-byte what the original XBE holds at that address**
# (`inc dword ptr [esi+0x1f0]`, confirmed with `inspect-jsrf.py data 0x00193D67 20`).
# Under the 8-byte reading `rip` is always zero, which is why all four archived dumps
# reported `eip = 0x00000000`: they were reading the wrong bytes, not a stopped guest.
#
# The packet is 344 bytes = 16 x 4 + 280 bytes of x87/SSE state this tool does not
# decode. 344 is not a multiple of 8, so the 8-byte reading was also misaligned from
# the first register onward.
REGISTER_ORDER = (
    'eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi',
    'eip', 'eflags', 'cs', 'ss', 'ds', 'es', 'fs', 'gs',
)

REGISTER_WIDTH = 4

# The guest's own disassembly names these, and they are now the packet's own names --
# so there is no alias layer to forget. Kept as a mapping for callers that ask for
# the 64-bit spelling of a 32-bit register.
GUEST_ALIASES = {
    'eax': 'rax', 'ebx': 'rbx', 'ecx': 'rcx', 'edx': 'rdx',
    'esi': 'rsi', 'edi': 'rdi', 'ebp': 'rbp', 'esp': 'rsp',
    'eip': 'rip',
}


class GdbStubError(RuntimeError):
    pass


class GdbStub:
    """A minimal GDB remote-serial client: stop, read registers, read memory."""

    def __init__(self, host: str = '127.0.0.1', port: int = 1234,
                 timeout: float = 10.0):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.sock.settimeout(timeout)
        self._buffer = b''

    def close(self) -> None:
        try:
            self.sock.close()
        except OSError:
            pass

    def __enter__(self) -> 'GdbStub':
        return self

    def __exit__(self, *_exc) -> None:
        self.close()

    def _send(self, payload: str) -> None:
        data = payload.encode('ascii')
        checksum = sum(data) & 0xFF
        self.sock.sendall(b'$' + data + b'#' + f'{checksum:02x}'.encode('ascii'))

    def _read_packet(self) -> str:
        """One `$...#xx` packet, acknowledging it.

        A leading `+`/`-` is a bare acknowledgement, not a packet; the stub emits
        them for its own packets, and treating one as data would misalign every
        later read.
        """
        deadline = time.time() + self.sock.gettimeout()
        while time.time() < deadline:
            start = self._buffer.find(b'$')
            if start < 0:
                try:
                    chunk = self.sock.recv(65536)
                except socket.timeout:
                    raise GdbStubError('timed out waiting for a packet')
                if not chunk:
                    raise GdbStubError('the stub closed the connection')
                self._buffer += chunk
                continue
            end = self._buffer.find(b'#', start)
            if end < 0 or len(self._buffer) < end + 3:
                try:
                    chunk = self.sock.recv(65536)
                except socket.timeout:
                    raise GdbStubError('timed out waiting for a packet body')
                if not chunk:
                    raise GdbStubError('the stub closed the connection')
                self._buffer += chunk
                continue
            payload = self._buffer[start + 1:end].decode('ascii', 'replace')
            self._buffer = self._buffer[end + 3:]
            self.sock.sendall(b'+')
            return payload
        raise GdbStubError('no packet arrived before the deadline')

    def request(self, payload: str) -> str:
        self._send(payload)
        return self._read_packet()

    def stop_reason(self) -> str:
        """`?` -- why the target is stopped, and its thread id."""
        return self.request('?')

    def registers(self) -> dict[str, int]:
        """`g` -- the i386 register block, little-endian, in REGISTER_ORDER.

        **The packet is longer than the registers this decodes.** Measured: 344
        bytes = 16 registers x 4 bytes + 280 bytes of x87/SSE state. The extra
        bytes are left undecoded rather than padded or guessed at; a tool that
        invented values for them would be reporting state it never read.
        """
        payload = self.request('g')
        if payload.startswith('E'):
            raise GdbStubError(f'the stub refused the register read: {payload}')
        raw = bytes.fromhex(payload)
        expected = len(REGISTER_ORDER) * REGISTER_WIDTH
        if len(raw) < expected:
            # Decode what is there rather than padding with zeros, which would
            # fabricate register values.
            raise GdbStubError(
                f'register block is {len(raw)} bytes, expected at least {expected}')
        values = {}
        for index, name in enumerate(REGISTER_ORDER):
            offset = index * REGISTER_WIDTH
            values[name] = struct.unpack_from('<I', raw, offset)[0]
        # The undecoded remainder is REPORTED, so a reader can tell a packet this
        # tool understood from one it silently truncated.
        self.last_register_block_bytes = len(raw)
        self.last_register_undecoded_bytes = len(raw) - expected
        return values

    def read_memory(self, address: int, length: int) -> bytes:
        """`m` -- read guest memory. Read-only by construction."""
        payload = self.request(f'm{address:x},{length:x}')
        if payload.startswith('E'):
            raise GdbStubError(f'the stub refused the read at 0x{address:08X}: '
                               f'{payload}')
        return bytes.fromhex(payload)

    def detach(self) -> None:
        """`D` -- let the guest continue without us. Not `k`, which kills it."""
        try:
            self.request('D')
        except (GdbStubError, OSError):
            pass


def guest_view(registers: dict[str, int]) -> dict[str, int]:
    """The guest's register values, which are the packet's own values.

    The registers are already 32-bit, so there is no truncation to perform and no
    alias layer for a consumer to forget. `GUEST_ALIASES` is kept so a caller can
    ask for the 64-bit spelling of a 32-bit register and get the same number, which
    is what the previous version's aliasing was for -- but the direction is now the
    safe one: the packet's names are the guest's names.
    """
    view: dict[str, int] = dict(registers)
    for guest_name, wide_name in GUEST_ALIASES.items():
        if guest_name in registers:
            view[wide_name] = registers[guest_name]
    return view


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, default=1234)
    parser.add_argument('--host', default='127.0.0.1')
    parser.add_argument('--read', action='append', default=[],
                        metavar='VA:LENGTH',
                        help='guest VA and length to dump, e.g. 0x001C3F60:64')
    parser.add_argument('--out', type=Path, help='write the dump JSON here')
    parser.add_argument('--json', action='store_true')
    parser.add_argument('--leave-running', action='store_true',
                        help='detach instead of terminating the stub connection')
    args = parser.parse_args()

    reads: list[tuple[int, int]] = []
    for spec in args.read:
        try:
            address, length = spec.split(':')
            reads.append((int(address, 0), int(length, 0)))
        except ValueError:
            print(f'bad --read {spec!r}; expected VA:LENGTH', file=sys.stderr)
            return 2

    record: dict = {
        'tool': 'jsrf-xemu-gdbstub/1',
        'generated_utc': datetime.now(timezone.utc).isoformat(),
        'endpoint': f'{args.host}:{args.port}',
        'note': ('Read-only: the packets used are ?, g, m and D. No packet that '
                 'writes guest memory or registers is ever sent, so the oracle '
                 'cannot change what it measures.'),
    }

    try:
        with GdbStub(args.host, args.port) as stub:
            record['stop_reason'] = stub.stop_reason()
            registers = stub.registers()
            record['register_block_bytes'] = getattr(
                stub, 'last_register_block_bytes', None)
            record['register_undecoded_bytes'] = getattr(
                stub, 'last_register_undecoded_bytes', None)
            record['register_layout'] = (
                f'i386, {REGISTER_WIDTH}-byte slots, '
                f'{len(REGISTER_ORDER)} registers')
            # The packet's own names ARE the guest's names, so one mapping is
            # recorded and the redundant `registers_64` spelling is gone -- it was
            # the label that made a wrong decode look authoritative.
            record['guest_registers'] = {k: f'0x{v:08X}'
                                         for k, v in guest_view(registers).items()}
            record['reads'] = []
            for address, length in reads:
                try:
                    data = stub.read_memory(address, length)
                except GdbStubError as error:
                    record['reads'].append({'address': f'0x{address:08X}',
                                            'length': length,
                                            'error': str(error)})
                    continue
                record['reads'].append({
                    'address': f'0x{address:08X}',
                    'length': length,
                    'bytes': len(data),
                    'hex': data.hex(),
                })
            if not args.leave_running:
                stub.detach()
        record['result'] = 'OK'
    except (OSError, GdbStubError) as error:
        record['result'] = 'FAILED'
        record['error'] = str(error)

    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(record, indent=2, sort_keys=True),
                            encoding='utf-8')

    if args.json:
        print(json.dumps(record, indent=2, sort_keys=True))
    elif record['result'] == 'OK':
        print(f'xemu gdbstub at {record["endpoint"]}')
        print(f'  stop reason : {record["stop_reason"]}')
        guest = record['guest_registers']
        # Grouped for reading; a register the stub did not return is shown as
        # UNKNOWN rather than omitted, so a short register block cannot look like
        # a register that simply was not interesting.
        for group in (('eip', 'esp', 'ebp', 'eflags'),
                      ('eax', 'ebx', 'ecx', 'edx'),
                      ('esi', 'edi')):
            print('  ' + '  '.join(
                f'{name}={guest.get(name, "UNKNOWN")}' for name in group))
        for entry in record['reads']:
            if 'error' in entry:
                print(f'  read {entry["address"]}: FAILED - {entry["error"]}')
            else:
                print(f'  read {entry["address"]} ({entry["bytes"]} bytes): '
                      f'{entry["hex"][:64]}{"..." if entry["bytes"] > 32 else ""}')
    else:
        print(f'xemu gdbstub FAILED: {record["error"]}', file=sys.stderr)

    return 0 if record['result'] == 'OK' else 2


if __name__ == '__main__':
    sys.exit(main())
