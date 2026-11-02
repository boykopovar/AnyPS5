from pathlib import Path
import os
import shutil
import struct
import subprocess
import sys
import tempfile

from guest_import_fixtures import executable
from test_guest_intel_trampolines import main_fixture, pe_sections
from test_guest_module_args import ARGP, ARGS, GETTER, RETURNED, SETTER, load_and_record, macho_binds
from test_guest_module_directories import needed_libraries
from test_guest_module_dir import pe_export_names
from test_guest_symbol_names import dynamic_tags, file_offset, imported_symbols, module_symbols

PREINIT_SLOT = 0x2900
SHARED = 0x100
STUB = b'\x48\x01\xc8\x31\xff\x31\xf6\x31\xd2\xff\xd0'
THREAD_POINTER = b'\x64\x48\x8b\x04\x25\0\0\0\0'


def zero_init_module(preinit, base=0):
    image = bytearray(0x3000)
    image[:16] = b'\x7fELF\x02\x01\x01' + bytes(9)
    struct.pack_into('<HHIQQQIHHHHHH', image, 16,
                     3, 62, 1, 0, 64, 0, 0, 64, 56, 5, 0, 0, 0)
    struct.pack_into('<IIQQQQQQ', image, 64, 1, 5, 0x1000, base, base, 0x1000, 0x1000, 0x1000)
    struct.pack_into('<IIQQQQQQ', image, 176, 0x6474e551, 6, 0, 0, 0, 0, 0, 16)
    struct.pack_into('<IIQQQQQQ', image, 232, 1, 6, 0x2000, 0x2000, 0x2000, 0x1000, 0x1000, 0x1000)
    struct.pack_into('<IIQQQQQQ', image, 288, 7, 4, 0x2a00, 0x2a00, 0x2a00, 8, 8, 8)
    strings = b'\0RecordArgs\0libc.prx\0shared\0'
    relocations = [(0x2820, (1 << 32) | 6, 0)] + ([(PREINIT_SLOT, 8, base + SHARED)] if preinit else [])
    tags = [(5, 0x2600), (10, len(strings)), (6, 0x2400), (11, 24), (4, 0x2500),
            (7, 0x2700), (8, 24 * len(relocations)), (9, 24), (1, 12), (12, 0)]
    if preinit:
        tags += [(32, PREINIT_SLOT), (33, 8)]
    tags.append((0, 0))
    struct.pack_into('<IIQQQQQQ', image, 120, 2, 6, 0x2000, 0x2000, 0x2000,
                     len(tags) * 16, len(tags) * 16, 8)
    for index, tag in enumerate(tags):
        struct.pack_into('<qQ', image, 0x2000 + index * 16, *tag)
    image[0x2600:0x2600 + len(strings)] = strings
    struct.pack_into('<IBBHQQ', image, 0x2418, 1, 0x12, 0, 0, 0, 0)
    struct.pack_into('<IBBHQQ', image, 0x2430, 21, 0x12, 0, 1, base + SHARED, 6)
    struct.pack_into('<II', image, 0x2500, 1, 3)
    for index, relocation in enumerate(relocations):
        struct.pack_into('<QQq', image, 0x2700 + index * 24, *relocation)
    code = THREAD_POINTER + b'\x48\x83\xec\x08\xff\x15' + struct.pack('<i', 0x2820 - base - len(THREAD_POINTER) - 10)
    code += b'\x48\x83\xc4\x08\xb8' + struct.pack('<I', RETURNED) + b'\xc3'
    image[0x1000:0x1000 + len(code)] = code
    image[0x1000 + SHARED:0x1000 + SHARED + 6] = b'\xb8\x2a\0\0\0\xc3'
    return image


def pe_word(data, rva):
    for _, start, size, raw, _ in pe_sections(data):
        if start <= rva < start + max(size, 0x1000):
            return struct.unpack_from('<I', data, raw + rva - start)[0] if rva + 4 <= start + size else 0
    raise AssertionError(f'Unmapped RVA: {rva:#x}')


def deferred_entry(data):
    header, = struct.unpack_from('<I', data, 0x3C)
    directory, = struct.unpack_from('<I', data, header + 24 + 112)
    index = pe_export_names(data).index('__aps5_guest_initialize')
    ordinal = pe_word(data, pe_word(data, directory + 36) + index * 2) & 0xffff
    return pe_word(data, pe_word(data, pe_word(data, directory + 28) + ordinal * 4))


def convert(relinker, work, preinit, options, base=0):
    modules = work / 'sce_module'
    modules.mkdir(parents=True)
    name = 'zero.prx'
    (modules / name).write_bytes(zero_init_module(preinit, base))
    source = work / 'input.elf'
    source.write_bytes(executable(name))
    output = work / ('output.exe' if '--windows' in options else 'output.elf')
    result = subprocess.run([str(relinker), '--skip-syscall-check', *options, str(source), str(output)],
                            capture_output=True, text=True, timeout=30)
    if base != 0:
        assert result.returncode != 0, (options, result.stdout, result.stderr)
        assert 'Unmapped or inaccessible guest address 0' in result.stdout + result.stderr, (options, result.stdout, result.stderr)
        return None, None
    assert result.returncode == 0, (preinit, options, result.stdout, result.stderr)
    return output, work / 'app0' / 'sce_module' / (name + '.guest.prx')


def check_deferred_linux(relinker, work, host, preinit, has_init, native):
    (work / 'sce_module').mkdir(parents=True)
    plugins = work / 'plugins'
    plugins.mkdir()
    source = work / 'input.elf'
    source.write_bytes(main_fixture())
    image = zero_init_module(preinit)
    if not has_init:
        for position in range(0x2000, 0x2300, 16):
            if struct.unpack_from('<q', image, position)[0] == 12:
                struct.pack_into('<q', image, position, 21)
                break
        else:
            raise AssertionError('Fixture has no DT_INIT tag to remove')
    (plugins / 'zero.prx').write_bytes(image)
    libraries = work / 'libs'
    libraries.mkdir()
    provider = libraries / 'libc.prx'
    shutil.copyfile(host, provider)
    shutil.copyfile(host, libraries / 'libkernel.prx')
    result = subprocess.run([str(relinker), '--skip-syscall-check', '--module-dir', 'plugins',
                             '--rpath', '$ORIGIN/libs', str(source), str(work / 'output.elf')],
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, (preinit, has_init, result.stdout, result.stderr)
    converted = work / 'app0/plugins/zero.prx.guest.prx'
    image = converted.read_bytes()
    assert ('libkernel.prx' in needed_libraries(image)) == has_init, 'Deferred initializer dependency does not match DT_INIT presence'
    tags = dynamic_tags(image)
    assert tags.get(12, 0) == 0 and tags.get(25, 0) == 0, 'Deferred Linux module has an eager initializer'
    index = module_symbols(image).index('__aps5_guest_lifecycle_nid_no_patch') + 1
    table, = struct.unpack_from('<Q', image, file_offset(image, tags[6]) + index * 24 + 8)
    relocations = [struct.unpack_from('<QQq', image, file_offset(image, tags[7]) + offset)
                   for offset in range(0, tags[8], tags[9])]
    init_relocations = [entry for entry in relocations if entry[0] == table + 16]
    expected = [(table + 16, 8, 0)] if has_init else []
    assert init_relocations == expected, (preinit, has_init, init_relocations, expected)
    if native and has_init:
        load_and_record(work, provider, converted)


def main():
    if sys.argv[1] == '--load':
        import ctypes
        host = ctypes.CDLL(sys.argv[2])
        host.SetPendingArgs.argtypes = [ctypes.c_size_t, ctypes.c_void_p]
        host.SetPendingArgs(ARGS, ARGP)
        ctypes.CDLL(sys.argv[3])
        return
    relinker = Path(sys.argv[1]).resolve()
    host = Path(sys.argv[2]).resolve()
    linux = sys.platform == 'linux' and os.uname().machine == 'x86_64'
    with tempfile.TemporaryDirectory(prefix='anyps5-guest-zero-init-') as directory:
        work = Path(directory)
        for preinit in (True, False):
            _, converted = convert(relinker, work / f'linux-{preinit}', preinit, [])
            image = converted.read_bytes()
            assert GETTER in imported_symbols(image), (preinit, imported_symbols(image))
            assert SETTER in imported_symbols(image), (preinit, imported_symbols(image))
            assert dynamic_tags(image).get(12, 0) != 0, (preinit, dynamic_tags(image))
            assert 'libkernel.prx' in needed_libraries(image), (preinit, needed_libraries(image))
            _, mach_converted = convert(relinker, work / f'macos-{preinit}', preinit, ['--macos'])
            binds = macho_binds(mach_converted.read_bytes())
            assert ('_' + GETTER).encode() in binds, f'the macOS module does not bind {GETTER}'
            assert ('_' + SETTER).encode() in binds, f'the macOS module does not bind {SETTER}'
            assert THREAD_POINTER not in mach_converted.read_bytes(), 'the macOS module keeps the initializer thread pointer load'
            windows, windows_converted = convert(relinker, work / f'windows-{preinit}', preinit, ['--windows'])
            assert b'\xb9' + struct.pack('<I', 0x10000) + STUB in windows.read_bytes(), 'the Windows startup does not call the initializer at RVA 0x10000'
            assert THREAD_POINTER not in windows_converted.read_bytes(), 'the Windows module keeps the initializer thread pointer load'
            deferred = work / f'deferred-{preinit}'
            (deferred / 'sce_module').mkdir(parents=True)
            (deferred / 'plugins').mkdir()
            (deferred / 'plugins' / 'zero.prx').write_bytes(zero_init_module(preinit))
            (deferred / 'input.elf').write_bytes(main_fixture())
            result = subprocess.run([str(relinker), '--windows', '--skip-syscall-check', '--module-dir', 'plugins',
                                     str(deferred / 'input.elf'), str(deferred / 'output.exe')],
                                    capture_output=True, text=True, timeout=30)
            assert result.returncode == 0, (preinit, result.stdout, result.stderr)
            entry = deferred_entry((deferred / 'app0' / 'plugins' / 'zero.prx.guest.prx').read_bytes())
            assert entry != 0, 'the deferred Windows module has no initializer'
            if linux:
                case = converted.parents[2]
                libraries = case / 'libs'
                libraries.mkdir()
                provider = libraries / 'libc.prx'
                shutil.copyfile(host, provider)
                shutil.copyfile(host, libraries / 'libkernel.prx')
                events = case / 'events.txt'
                run = subprocess.run([sys.executable, __file__, '--load', str(provider), str(converted)],
                                     env={**os.environ, 'ANYPS5_GUEST_MODULE_ARGS': str(events)},
                                     capture_output=True, text=True, timeout=30)
                assert run.returncode == 0, (preinit, run.returncode, run.stdout, run.stderr)
                recorded = events.read_text()
                assert f'args={ARGS:016x} argp={ARGP:016x}\nresult={RETURNED}\n' == recorded, (preinit, recorded)
        for preinit, has_init in ((False, True), (True, True), (False, False)):
            check_deferred_linux(relinker, work / f'linux-runtime-{preinit}-{has_init}',
                                 host, preinit, has_init, linux)
        for index, options in enumerate(([], ['--macos'], ['--windows'])):
            convert(relinker, work / f'outside-{index}', False, options, 0x1000)
    print('Guest zero initializer tests passed' if linux else 'Guest zero initializer tests passed: native execution skipped')


if __name__ == '__main__':
    main()
