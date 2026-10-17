from pathlib import Path
import os
import platform
import shutil
import struct
import subprocess
import sys
import tempfile

from test_guest_symbol_names import dynamic_tags, file_offset, imported_symbols, module_symbols
from test_macos_output import MachO
from test_guest_intel_trampolines import main_fixture, pe_sections
from test_windows_import_modules import executable

ARGS = 0x1122334455667788
ARGP = 0x99AABBCCDDEEFF00
RETURNED = 0x5A17

GETTER = '__aps5_get_pending_module_args_nid_no_patch'
SETTER = '__aps5_set_module_init_result_nid_no_patch'

SHARED = 0x1000
PREINIT0 = 0x1010
PREINIT1 = 0x1030
INIT = 0x1050
RECORD_GOT = 0x2820
PREINIT_SLOTS = 0x2840
FLAG = 0x2820
IMAGE_BASE = 0x140000000
LOAD_RVA = 0x10000
CALL_SLOT = b'\x48\x8b\x05'


def elf_header(image, program_headers=4):
    image[:16] = b'\x7fELF\x02\x01\x01' + bytes(9)
    struct.pack_into('<HHIQQQIHHHHHH', image, 16,
                     3, 62, 1, 0, 64, 0, 0, 64, 56, program_headers, 0, 0, 0)
    struct.pack_into('<IIQQQQQQ', image, 64, 1, 5, 0, 0, 0, 0x2000, 0x2000, 0x1000)
    struct.pack_into('<IIQQQQQQ', image, 176, 0x6474e551, 6, 0, 0, 0, 0, 0, 16)
    struct.pack_into('<IIQQQQQQ', image, 232, 1, 6, 0x2000, 0x2000, 0x2000, 0x1000, 0x1000, 0x1000)


def recording_module():
    image = bytearray(0x3000)
    elf_header(image)
    strings = b'\0RecordArgs\0libc.prx\0shared\0'
    tags = [(5, 0x2600), (10, len(strings)), (6, 0x2400), (11, 24), (4, 0x2500),
            (7, 0x2700), (8, 72), (9, 24), (1, 12), (12, INIT), (32, PREINIT_SLOTS), (33, 16), (0, 0)]
    struct.pack_into('<IIQQQQQQ', image, 120, 2, 6, 0x2000, 0x2000, 0x2000,
                     len(tags) * 16, len(tags) * 16, 8)
    for index, tag in enumerate(tags):
        struct.pack_into('<qQ', image, 0x2000 + index * 16, *tag)
    image[0x2600:0x2600 + len(strings)] = strings
    struct.pack_into('<IBBHQQ', image, 0x2418, 1, 0x12, 0, 0, 0, 0)
    struct.pack_into('<IBBHQQ', image, 0x2430, 21, 0x12, 0, 1, SHARED, 6)
    struct.pack_into('<IIII', image, 0x2500, 1, 3, 0, 0)
    struct.pack_into('<QQq', image, 0x2700, RECORD_GOT, (1 << 32) | 6, 0)
    struct.pack_into('<QQq', image, 0x2718, PREINIT_SLOTS, 8, PREINIT0)
    struct.pack_into('<QQq', image, 0x2730, PREINIT_SLOTS + 8, 8, PREINIT1)
    image[SHARED:SHARED + 6] = b'\xb8\x2a\x00\x00\x00\xc3'
    for address in (PREINIT0, PREINIT1):
        code = b'\x48\x83\xec\x08\xff\x15' + struct.pack('<i', RECORD_GOT - (address + 10)) + b'\x48\x83\xc4\x08\xc3'
        image[address:address + len(code)] = code
    code = b'\x48\x83\xec\x08\xff\x15' + struct.pack('<i', RECORD_GOT - (INIT + 10))
    code += b'\x48\x83\xc4\x08\xb8' + struct.pack('<I', RETURNED) + b'\xc3'
    image[INIT:INIT + len(code)] = code
    return image


def flag_module():
    image = bytearray(0x3000)
    elf_header(image)
    strings = b'\0shared\0'
    tags = [(5, 0x2600), (10, len(strings)), (6, 0x2400), (11, 24), (4, 0x2500),
            (7, 0x2700), (8, 48), (9, 24), (12, INIT), (32, PREINIT_SLOTS), (33, 16), (0, 0)]
    struct.pack_into('<IIQQQQQQ', image, 120, 2, 6, 0x2000, 0x2000, 0x2000,
                     len(tags) * 16, len(tags) * 16, 8)
    for index, tag in enumerate(tags):
        struct.pack_into('<qQ', image, 0x2000 + index * 16, *tag)
    image[0x2600:0x2600 + len(strings)] = strings
    struct.pack_into('<IBBHQQ', image, 0x2418, 1, 0x12, 0, 1, SHARED, 6)
    struct.pack_into('<IIII', image, 0x2500, 1, 2, 0, 0)
    struct.pack_into('<QQq', image, 0x2700, PREINIT_SLOTS, 8, PREINIT0)
    struct.pack_into('<QQq', image, 0x2718, PREINIT_SLOTS + 8, 8, PREINIT1)
    image[SHARED:SHARED + 6] = b'\xb8\x2a\x00\x00\x00\xc3'
    for address in (PREINIT0, PREINIT1):
        code = b'\xc7\x05' + struct.pack('<i', FLAG - (address + 10)) + b'\x01\x00\x00\x00\xc3'
        image[address:address + len(code)] = code
    code = b'\x8b\x05' + struct.pack('<i', FLAG - (INIT + 6))
    code += b'\x83\xf8\x01\x74\x02\x0f\x0b\xb8\x2a\x00\x00\x00\xc3'
    image[INIT:INIT + len(code)] = code
    return image


def load_and_record(work, provider, module):
    events = work / 'events.txt'
    run = subprocess.run([sys.executable, __file__, '--load', str(provider), str(module)],
                         env={**os.environ, 'ANYPS5_GUEST_MODULE_ARGS': str(events)},
                         capture_output=True, text=True, timeout=30)
    assert run.returncode == 0, (run.returncode, run.stdout, run.stderr)
    lines = events.read_text().splitlines()
    assert len(lines) == 4, lines
    assert lines[0] == 'args=0000000000000000 argp=0000000000000000', lines
    assert lines[1] == 'args=0000000000000000 argp=0000000000000000', lines
    assert lines[2] == f'args={ARGS:016x} argp={ARGP:016x}', lines
    assert lines[3] == f'result={RETURNED}', lines


def check_linux_stub(image, stub, slots):
    offset = file_offset(image, stub)
    assert image[offset] == 0x50, image[offset:offset + 48]
    for index, slot in enumerate(slots):
        base = offset + 1 + 20 * index
        assert image[base:base + 3] == CALL_SLOT, image[base:base + 20]
        displacement, = struct.unpack_from('<i', image, base + 3)
        assert stub + 1 + 20 * index + 7 + displacement == slot, (stub, index, displacement)
        assert image[base + 7:base + 20] == b'\x48\x85\xc0\x74\x08\x31\xff\x31\xf6\x31\xd2\xff\xd0', image[base + 7:base + 20]
    tail = offset + 1 + 20 * len(slots)
    assert image[tail] == 0x58, image[tail:tail + 7]
    assert image[tail + 1] == 0xe9, image[tail:tail + 7]
    displacement, = struct.unpack_from('<i', image, tail + 2)
    initializer = stub + (tail - offset) + 6 + displacement
    assert initializer != stub, initializer
    entry = file_offset(image, initializer)
    assert image[entry] == 0x50 and image[entry + 1:entry + 3] == b'\xff\x15', image[entry:entry + 8]


def check_macos_stub(data, slots):
    machine = MachO(data)

    def address_of(position):
        for segment in machine.segments:
            if segment['fileoff'] <= position < segment['fileoff'] + segment['filesize']:
                return segment['vmaddr'] + position - segment['fileoff']
        raise AssertionError(f'Unmapped file offset: {position:#x}')

    targets = []
    position = data.find(CALL_SLOT)
    while position >= 0:
        if data[position + 7:position + 20] == b'\x48\x85\xc0\x74\x08\x31\xff\x31\xf6\x31\xd2\xff\xd0':
            displacement, = struct.unpack_from('<i', data, position + 3)
            targets.append(address_of(position) + 7 + displacement)
        position = data.find(CALL_SLOT, position + 1)
    assert sorted(targets) == sorted(slots), targets


def check_pe_relocations(data, section, addresses):
    _, raw_size, raw_offset, _ = section
    found = set()
    position = 0
    while position + 8 <= raw_size:
        page, size = struct.unpack_from('<II', data, raw_offset + position)
        if size < 8:
            break
        for index in range(8, size, 2):
            entry, = struct.unpack_from('<H', data, raw_offset + position + index)
            if entry >> 12 == 10:
                found.add(page + (entry & 0xfff))
        position += size
    assert set(addresses) <= found, (sorted(found), addresses)


def check_windows_deferred(data, slots):
    sections = {name: (rva, raw_size, raw_offset, flags) for name, rva, raw_size, raw_offset, flags in pe_sections(data)}
    ginit_rva, ginit_size, ginit_raw, _ = sections[b'.ginit']
    ginitc_rva, ginitc_size, ginitc_raw, _ = sections[b'.ginitc']
    entry, count = struct.unpack_from('<II', data, ginit_raw)
    assert entry == ginitc_rva, (entry, ginitc_rva)
    assert count == 0, count
    code = data[ginitc_raw:ginitc_raw + ginitc_size]
    assert code[:15] == b'\x41\x54\x41\x55\x41\x56\x49\x89\xfc\x49\x89\xf5\x49\x89\xd6', code[:15]
    code_size = 40 + 23 * len(slots)
    restore = 15 + 23 * len(slots)
    assert code[restore:restore + 12] == b'\x4c\x89\xe7\x4c\x89\xee\x4c\x89\xf2\x48\x8b\x05', code[restore:restore + 16]
    assert code[restore + 16:restore + 18] == b'\xff\xd0', code[restore:restore + 25]
    assert code[restore + 18:restore + 25] == b'\x41\x5c\x41\x5d\x41\x5e\xc3', code[restore:restore + 25]
    slots_offset = code_size + (8 - code_size % 8) % 8
    slots_rva = ginitc_rva + slots_offset
    for index, slot in enumerate(slots):
        base = 15 + 23 * index
        assert code[base:base + 3] == CALL_SLOT, code[base:base + 23]
        assert code[base + 3 + 4:base + 3 + 4 + 4] == b'\x48\x8b\x00\x48', code[base:base + 23]
        displacement, = struct.unpack_from('<i', code, base + 3)
        assert ginitc_rva + base + 7 + displacement == slots_rva + 8 * index, (index, displacement)
        value, = struct.unpack_from('<Q', code, slots_offset + 8 * index)
        assert value == IMAGE_BASE + LOAD_RVA + slot, (value, slot)
    initializer, = struct.unpack_from('<Q', code, slots_offset + 8 * len(slots))
    assert initializer == IMAGE_BASE + LOAD_RVA + INIT, initializer
    check_pe_relocations(data, sections[b'.reloc'], [slots_rva + 8 * index for index in range(len(slots) + 1)])


def check_windows_startup(data, slots):
    sections = {name: (rva, raw_size, raw_offset, flags) for name, rva, raw_size, raw_offset, flags in pe_sections(data)}
    _, raw_size, raw_offset, _ = sections[b'.entry']
    code = data[raw_offset:raw_offset + raw_size]

    def position_of(address):
        marker = b'\xb9' + struct.pack('<I', address)
        found = code.find(marker)
        assert found >= 0, address
        return found

    initializer = position_of(LOAD_RVA + INIT)
    for slot in slots:
        assert position_of(LOAD_RVA + slot) < initializer, (slot, initializer)


def main():
    if sys.argv[1] == '--load':
        import ctypes
        host = ctypes.CDLL(sys.argv[2], mode=ctypes.RTLD_GLOBAL if sys.platform == 'darwin' else ctypes.DEFAULT_MODE)
        host.SetPendingArgs.argtypes = [ctypes.c_size_t, ctypes.c_void_p]
        host.SetPendingArgs(ARGS, ARGP)
        ctypes.CDLL(sys.argv[3])
        return
    relinker = Path(sys.argv[1]).resolve()
    host = Path(sys.argv[2]).resolve()
    linux = sys.platform == 'linux' and os.uname().machine == 'x86_64'
    macos = sys.platform == 'darwin' and platform.machine() == 'x86_64'
    with tempfile.TemporaryDirectory(prefix='anyps5-guest-preinit-') as directory:
        work = Path(directory)
        modules = work / 'sce_module'
        modules.mkdir()
        name = 'preinit.prx'
        (modules / name).write_bytes(recording_module())
        source = work / 'input.elf'
        source.write_bytes(executable(name))
        output = work / 'output.elf'
        result = subprocess.run([str(relinker), '--skip-syscall-check', str(source), str(output)],
                                capture_output=True, text=True, timeout=30)
        assert result.returncode == 0, (result.stdout, result.stderr)
        converted = output.parent / 'app0' / 'sce_module' / (name + '.guest.prx')
        image = converted.read_bytes()
        assert GETTER in imported_symbols(image), imported_symbols(image)
        assert SETTER in imported_symbols(image), imported_symbols(image)
        assert 'shared#guest' in module_symbols(image), module_symbols(image)
        tags = dynamic_tags(image)
        check_linux_stub(image, tags[12], [PREINIT_SLOTS, PREINIT_SLOTS + 8])
        if linux:
            libraries = work / 'libs'
            libraries.mkdir()
            shutil.copyfile(host, libraries / 'libc.prx')
            load_and_record(work, libraries / 'libc.prx', converted)
        mach = work / 'macos'
        mach_modules = mach / 'sce_module'
        mach_modules.mkdir(parents=True)
        (mach_modules / name).write_bytes(recording_module())
        mach_source = mach / 'input.elf'
        mach_source.write_bytes(executable(name))
        mach_result = subprocess.run([str(relinker), '--macos', '--skip-syscall-check', str(mach_source), str(mach / 'output')],
                                     capture_output=True, text=True, timeout=30)
        assert mach_result.returncode == 0, (mach_result.stdout, mach_result.stderr)
        mach_converted = mach / 'app0' / 'sce_module' / (name + '.guest.prx')
        check_macos_stub(mach_converted.read_bytes(), [IMAGE_BASE + LOAD_RVA + PREINIT_SLOTS, IMAGE_BASE + LOAD_RVA + PREINIT_SLOTS + 8])
        windows = work / 'windows'
        win_modules = windows / 'sce_module'
        win_modules.mkdir(parents=True)
        (win_modules / name).write_bytes(flag_module())
        win_source = windows / 'input.elf'
        win_source.write_bytes(executable(name))
        win_result = subprocess.run([str(relinker), '--windows', '--skip-syscall-check', str(win_source), str(windows / 'output.exe')],
                                    capture_output=True, text=True, timeout=30)
        assert win_result.returncode == 0, (win_result.stdout, win_result.stderr)
        win_converted = windows / 'app0' / 'sce_module' / (name + '.guest.prx')
        check_windows_startup((windows / 'output.exe').read_bytes(), [PREINIT_SLOTS, PREINIT_SLOTS + 8])
        deferred = work / 'deferred'
        (deferred / 'sce_module').mkdir(parents=True)
        plugins = deferred / 'Media' / 'Plugins'
        plugins.mkdir(parents=True)
        (plugins / name).write_bytes(flag_module())
        deferred_source = deferred / 'input.elf'
        deferred_source.write_bytes(main_fixture())
        deferred_result = subprocess.run([str(relinker), '--windows', '--skip-syscall-check',
                                          '--module-dir', 'Media/Plugins', str(deferred_source), str(deferred / 'output.exe')],
                                         capture_output=True, text=True, timeout=30)
        assert deferred_result.returncode == 0, (deferred_result.stdout, deferred_result.stderr)
        deferred_converted = deferred / 'app0' / 'Media' / 'Plugins' / (name + '.guest.prx')
        check_windows_deferred(deferred_converted.read_bytes(), [PREINIT_SLOTS, PREINIT_SLOTS + 8])
        if os.name == 'nt':
            run = subprocess.run([str(windows / 'output.exe')], capture_output=True, text=True, timeout=30)
            assert run.returncode == 42, (run.returncode, run.stdout, run.stderr)
        if linux or macos or os.name == 'nt':
            print('Guest preinit tests passed')
        else:
            print('Guest preinit tests passed: native execution skipped')


if __name__ == '__main__':
    main()
