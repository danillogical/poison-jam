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

# x86-64 RSP register order, as returned by `g`.
REGISTER_ORDER = (
    'rax', 'rbx', 'rcx', 'rdx', 'rsi', 'rdi', 'rbp', 'rsp',
    'r8', 'r9', 'r10', 'r11', 'r12', 'r13', 'r14', 'r15',
    'rip', 'eflags', 'cs', 'ss', 'ds', 'es', 'fs', 'gs',
)

# The guest is 32-bit; these are the names the guest's own disassembly uses.
# Every 64-bit register gets a 32-bit alias, because the guest's state lives in
# the low half and a dump that offered only `rax` would make every consumer
# remember the mapping -- and a consumer that forgot would compare a guest value
# against a host-width one.
GUEST_ALIASES = {
    'rax': 'eax', 'rbx': 'ebx', 'rcx': 'ecx', 'rdx': 'edx',
    'rsi': 'esi', 'rdi': 'edi', 'rbp': 'ebp', 'rsp': 'esp',
    'r8': 'r8d', 'r9': 'r9d', 'r10': 'r10d', 'r11': 'r11d',
    'r12': 'r12d', 'r13': 'r13d', 'r14': 'r14d', 'r15': 'r15d',
    'rip': 'eip',
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
        """`g` -- every register, little-endian, in REGISTER_ORDER."""
        payload = self.request('g')
        if payload.startswith('E'):
            raise GdbStubError(f'the stub refused the register read: {payload}')
        raw = bytes.fromhex(payload)
        width = 8
        expected = len(REGISTER_ORDER) * width
        if len(raw) < expected:
            # Some stubs return fewer; decode what is there rather than padding
            # with zeros, which would fabricate register values.
            raise GdbStubError(
                f'register block is {len(raw)} bytes, expected {expected}')
        values = {}
        for index, name in enumerate(REGISTER_ORDER):
            values[name] = struct.unpack_from('<Q', raw, index * width)[0]
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
    """The 32-bit view of the registers, which is what the guest actually sees.

    Every register keeps a **guest-width alias** as well as its RSP name, because
    the guest is 32-bit and its own disassembly names `eax`, `eip`, `esp`. A dump
    that offered only `rax`/`rip` would force every consumer to remember the
    mapping, and a consumer that forgot would compare a guest address against a
    host-width value. Both spellings are present and equal in the low 32 bits.
    """
    view: dict[str, int] = {}
    for name, value in registers.items():
        truncated = value & 0xFFFFFFFF
        view[name] = truncated
        guest_name = GUEST_ALIASES.get(name)
        if guest_name:
            view[guest_name] = truncated
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
            record['registers_64'] = {k: f'0x{v:016X}' for k, v in registers.items()}
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
