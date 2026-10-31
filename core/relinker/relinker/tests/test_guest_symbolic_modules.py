from pathlib import Path
import os
import struct
import subprocess
import sys

from guest_import_fixtures import dynamic, fixture_directory
from test_guest_intel_trampolines import PLAIN_SITE, guest_fixture, main_fixture

ENTRY = 0x1010
SHARED = 0x1000
CALL_SLOTS = (0x23a0, 0x23a8, 0x23b0)
TLS_SLOT = 0x23b8


def module(value, tls_value, identity, entry_symbol, symbolic):
    image = guest_fixture(PLAIN_SITE, symbolic=symbolic)
    strings = b'\0shared#A#B\0' + entry_symbol.encode() + b'#A#B\0tls_value#A#B\0'
    entry_name = strings.index(entry_symbol.encode())
    tls_name = strings.index(b'tls_value')
    identity_offset = len(strings)
    strings += identity.encode() + b'\0'
    assert len(strings) <= 0x40
    image[0x800:0x800 + len(strings)] = strings
    tags = [(5, 0x2200), (10, len(strings)), (6, 0x2280), (11, 24),
            (4, 0x2240), (7, 0x2300), (8, 24 * (len(CALL_SLOTS) + 1)), (9, 24),
            (0x61000043, (1 << 48) | (0x101 << 32) | identity_offset)]
    if symbolic:
        tags.append((30, 2))
    dynamic(image, 0x600, 176, tags)
    struct.pack_into('<H', image, 56, 4)
    struct.pack_into('<IIQQQQQQ', image, 232, 7, 4, 0x980, 0x2380, 0x2380, 8, 8, 8)
    struct.pack_into('<Q', image, 0x980, tls_value)
    struct.pack_into('<IIIIIII', image, 0x840, 1, 4, 1, 0, 2, 3, 0)
    shared = b'\xb8' + struct.pack('<I', value) + b'\xc3'
    entry = bytearray(b'\x53')
    for index, slot in enumerate(CALL_SLOTS):
        call = ENTRY + len(entry)
        entry += b'\xff\x15' + struct.pack('<i', slot - (call + 6))
        entry += b'\x89\xc3' if index == 0 else b'\x01\xc3'
    load = ENTRY + len(entry)
    entry += b'\x48\x8b\x05' + struct.pack('<i', TLS_SLOT - (load + 7))
    entry += b'\x64\x48\x8b\x80\0\0\0\0' + b'\x01\xd8\x5b\xc3'
    image[0x400:0x400 + len(shared)] = shared
    image[0x410:0x410 + len(entry)] = entry
    struct.pack_into('<IBBHQQ', image, 0x898, 1, 0x12, 0, 1, SHARED, len(shared))
    struct.pack_into('<IBBHQQ', image, 0x8b0, entry_name, 0x12, 0, 1, ENTRY, len(entry))
    struct.pack_into('<IBBHQQ', image, 0x8c8, tls_name, 0x16, 0, 1, 0, 8)
    for index, (slot, kind) in enumerate(zip(CALL_SLOTS, (6, 7, 1))):
        struct.pack_into('<QQq', image, 0x900 + 24 * index, slot, (1 << 32) | kind, 0)
    struct.pack_into('<QQq', image, 0x900 + 24 * len(CALL_SLOTS), TLS_SLOT, (3 << 32) | 18, 0)
    return image


def executable(first, second):
    image = main_fixture()
    struct.pack_into('<I', image, 68, 7)
    names = (b'first#A#B', b'second#A#C', first.encode() + b'.prx', second.encode() + b'.prx', first.encode(), second.encode())
    strings = b'\0'
    offsets = []
    for name in names:
        offsets.append(len(strings))
        strings += name + b'\0'
    image[0x4800:0x4800 + len(strings)] = strings
    tags = [(5, 0x4800), (10, len(strings)), (6, 0x4880), (11, 24),
            (4, 0x4840), (7, 0x4900), (8, 48), (9, 24),
            (1, offsets[2]), (1, offsets[3]),
            (0x61000045, (1 << 48) | offsets[4]), (0x61000045, (2 << 48) | offsets[5])]
    dynamic(image, 0x4600, 120, tags)
    struct.pack_into('<IIIIII', image, 0x4840, 1, 3, 1, 0, 2, 0)
    for index in range(2):
        struct.pack_into('<IBBHQQ', image, 0x4898 + 24 * index, offsets[index], 0x12, 0, 0, 0, 0)
        struct.pack_into('<QQq', image, 0x4900 + 24 * index, 0x4a20 + 8 * index, ((index + 1) << 32) | 6, 0)
    code = (b'\x53' + b'\xff\x15' + struct.pack('<i', 0x4a20 - 0x4007) + b'\x89\xc3'
            + b'\xff\x15' + struct.pack('<i', 0x4a28 - 0x400f) + b'\xc1\xe0\x04\x01\xd8\x5b\xc3')
    image[0x4000:0x4000 + len(code)] = code
    return image


def main():
    relinker = Path(sys.argv[1]).resolve()
    with fixture_directory('anyps5-symbolic-modules-') as directory:
        work = Path(directory)
        (work / 'sce_module').mkdir()
        (work / 'sce_module' / 'libFirst.prx').write_bytes(module(1, 4, 'libFirst', 'first', symbolic=False))
        (work / 'sce_module' / 'libSecond.prx').write_bytes(module(2, 8, 'libSecond', 'second', symbolic=True))
        source = work / 'input.elf'
        source.write_bytes(executable('libFirst', 'libSecond'))
        output = work / 'output.exe'
        result = subprocess.run([str(relinker), '--windows', str(source), str(output)], capture_output=True, text=True, timeout=30)
        assert result.returncode == 0, (result.stdout, result.stderr)
        modules = work / 'app0' / 'sce_module'
        for name in ('libFirst', 'libSecond'):
            assert (modules / (name + '.prx.guest.prx')).read_bytes().startswith(b'MZ'), name
        if os.name == 'nt':
            run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
            assert run.returncode == (3 * 1 + 4) + (3 * 2 + 8) * 16, (run.returncode, run.stdout, run.stderr)
    print('Guest symbolic module tests passed')


if __name__ == '__main__':
    main()
