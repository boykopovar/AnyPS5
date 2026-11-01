from pathlib import Path
import os
import platform
import shutil
import struct
import subprocess
import sys

from guest_import_fixtures import executable, fixture_directory
from test_guest_module_args import macos_loader, macho_binds, macho_x86_64
from test_guest_module_fini import fini_module
from test_guest_intel_trampolines import main_fixture
from test_guest_symbol_names import dynamic_tags, file_offset, module_symbols
from test_guest_zero_initializer import STUB, THREAD_POINTER


EXPECTED = 'fini=0000000000000000 argp=0000000000000000 callback=0000000000000000\n'


def finalizer_module(base, finalizer, tls):
    image = fini_module(False)
    struct.pack_into('<H', image, 56, 5 if tls else 4)
    struct.pack_into('<IIQQQQQQ', image, 64, 1, 5, 0x1000, base, base, 0x1000, 0x1000, 0x1000)
    struct.pack_into('<IIQQQQQQ', image, 288, 7, 4, 0x2a00, 0x2a00, 0x2a00, 8, 8, 8)
    struct.pack_into('<Q', image, 0x2430 + 8, base + 0x100)
    prefix = THREAD_POINTER if tls else b''
    code = prefix + b'\xff\x25' + struct.pack('<i', 0x2828 - base - len(prefix) - 6)
    image[0x1000:0x1000 + len(code)] = code
    image[0x1100:0x1106] = b'\xb8\x2a\0\0\0\xc3'
    tags = [(tag, finalizer if tag == 13 else value) for tag, value in dynamic_tags(image).items()
            if finalizer is not None or tag != 13]
    for index, tag in enumerate(tags + [(0, 0)]):
        struct.pack_into('<qQ', image, 0x2000 + index * 16, *tag)
    struct.pack_into('<QQ', image, 120 + 32, (len(tags) + 1) * 16, (len(tags) + 1) * 16)
    return image


def convert(relinker, case, options, base, finalizer, tls=True, deferred=False):
    modules = case / ('plugins' if deferred else 'sce_module')
    modules.mkdir(parents=True)
    (modules / 'fini.prx').write_bytes(finalizer_module(base, finalizer, tls))
    source, output = case / 'input.elf', case / ('output.exe' if '--windows' in options else 'output.elf')
    source.write_bytes(main_fixture() if deferred else executable('fini.prx'))
    module_options = ['--module-dir', 'plugins'] if deferred else []
    result = subprocess.run([str(relinker), '--skip-syscall-check', *options, *module_options, str(source), str(output)],
                            capture_output=True, text=True, timeout=10)
    if finalizer == 0 and base != 0:
        assert result.returncode != 0, (options, result.stdout, result.stderr)
        assert 'Unmapped or inaccessible guest address 0' in result.stdout + result.stderr, (options, result.stdout, result.stderr)
        return None, None
    assert result.returncode == 0, (options, finalizer, result.stdout, result.stderr)
    return output, modules.parent / 'app0' / modules.name / 'fini.prx.guest.prx'


def check_native(case, output, converted, host, compiler, present, system, loader, deferred=False):
    native = ('windows' if sys.platform == 'win32' else 'linux' if sys.platform == 'linux' else 'macos')
    if system != native or platform.machine().lower() not in ('amd64', 'x86_64', 'arm64'):
        return
    if system == 'linux' and platform.machine().lower() != 'x86_64':
        return
    if system == 'macos' and loader is None:
        return
    libraries = case / 'libs'
    libraries.mkdir()
    provider = libraries / 'libc.prx'
    shutil.copyfile(host, provider)
    shutil.copyfile(host, libraries / 'libkernel.prx')
    events = case / 'events.txt'
    if system == 'windows':
        for name in ('libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll'):
            dependency = compiler.parent / name
            if dependency.is_file():
                shutil.copyfile(dependency, libraries / name)
        command, expected_exit = [str(output)], 42
    elif system == 'macos':
        command, expected_exit = loader + [str(provider), str(converted), '0', '0'], 0
    else:
        command, expected_exit = [sys.executable, __file__, '--load-deferred' if deferred else '--load', str(provider), str(converted)], 0
    run = subprocess.run(command, cwd=case, env={**os.environ, 'ANYPS5_GUEST_MODULE_ARGS': str(events)},
                         capture_output=True, text=True, timeout=10)
    recorded = events.read_text() if events.exists() else ''
    assert run.returncode == expected_exit, (system, present, run.returncode, recorded, run.stdout, run.stderr)
    assert recorded == (EXPECTED if present else ''), (system, present, recorded)


def main():
    if sys.argv[1] in ('--load', '--load-deferred'):
        import ctypes
        import _ctypes
        host = ctypes.CDLL(sys.argv[2])
        module = ctypes.CDLL(sys.argv[3])
        if sys.argv[1] == '--load-deferred':
            table = (ctypes.c_uint64 * 8).in_dll(module, '__aps5_guest_lifecycle_nid_no_patch')
            assert table[0] == 0x415053354c494658 and table[6] == 0
            assert not Path(os.environ['ANYPS5_GUEST_MODULE_ARGS']).exists()
            table[6] = 2
        _ctypes.dlclose(module._handle)
        return
    relinker, host, compiler = (Path(value).resolve() for value in sys.argv[1:4])
    with fixture_directory('anyps5-zero-fini-') as directory:
        root = Path(directory)
        loader = macos_loader(root) if sys.platform == 'darwin' and macho_x86_64(host) else None
        for system, options in (('linux', []), ('windows', ['--windows']), ('macos', ['--macos'])):
            for name, base, finalizer in (('zero', 0, 0), ('absent', 0, None), ('nonzero', 0x1000, 0x1000)):
                case = root / system / name
                output, converted = convert(relinker, case, options, base, finalizer)
                image = converted.read_bytes()
                present = finalizer is not None
                if system == 'linux':
                    assert (dynamic_tags(image).get(13, 0) != 0) == present, (name, dynamic_tags(image))
                elif system == 'windows':
                    call = b'\xb9' + struct.pack('<I', 0x10000) + STUB
                    assert (call in output.read_bytes()) == present, name
                else:
                    assert (b'___cxa_atexit\0' in macho_binds(image)) == present, name
                if present and system != 'linux':
                    assert THREAD_POINTER not in image, (system, name, 'the finalizer thread pointer load was not lowered')
                if system == 'macos':
                    case = root / system / (name + '-native')
                    output, converted = convert(relinker, case, options, base, finalizer, tls=False)
                    binds = macho_binds(converted.read_bytes())
                    assert (b'___cxa_atexit\0' in binds) == present, name
                    assert b'_Aps5GuestTlsAllocate_nid_no_patch\0' not in binds, name
                    assert b'_Aps5GuestTlsRegister_nid_no_patch\0' not in binds, name
                check_native(case, output, converted, host, compiler, present, system, loader)
            convert(relinker, root / system / 'unmapped', options, 0x1000, 0)
        for name, base, finalizer in (('zero', 0, 0), ('absent', 0, None), ('nonzero', 0x1000, 0x1000)):
            case = root / 'linux-deferred' / name
            output, converted = convert(relinker, case, [], base, finalizer, tls=False, deferred=True)
            image = converted.read_bytes()
            tags = dynamic_tags(image)
            present = finalizer is not None
            assert (tags.get(13, 0) != 0) == present, (name, tags)
            index = module_symbols(image).index('__aps5_guest_lifecycle_nid_no_patch') + 1
            table, = struct.unpack_from('<Q', image, file_offset(image, tags[6]) + index * 24 + 8)
            relocations = [struct.unpack_from('<QQq', image, file_offset(image, tags[7]) + offset)
                           for offset in range(0, tags[8], tags[9])]
            fini_relocations = [entry for entry in relocations if entry[0] == table + 24]
            assert fini_relocations == ([(table + 24, 8, finalizer)] if present else []), (name, fini_relocations)
            check_native(case, output, converted, host, compiler, present, 'linux', loader, deferred=True)
    print('Guest zero finalizer: three output formats, deferred Linux, absent/nonzero controls and unmapped addresses passed')


if __name__ == '__main__':
    main()
