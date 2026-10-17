from pathlib import Path
import os
import platform
import shutil
import struct
import subprocess
import sys
import tempfile

from test_guest_symbol_names import dynamic_tags, imported_symbols, module_symbols
from test_macos_output import MachO
from test_windows_import_modules import executable

ARGS = 0x1122334455667788
ARGP = 0x99AABBCCDDEEFF00
RETURNED = 0x5A17
GETTER = '__aps5_get_pending_module_args_nid_no_patch'
SETTER = '__aps5_set_module_init_result_nid_no_patch'


def args_module():
    image = bytearray(0x3000)
    image[:16] = b'\x7fELF\x02\x01\x01' + bytes(9)
    struct.pack_into('<HHIQQQIHHHHHH', image, 16,
                     3, 62, 1, 0, 64, 0, 0, 64, 56, 4, 0, 0, 0)
    struct.pack_into('<IIQQQQQQ', image, 64, 1, 5, 0, 0, 0, 0x2000, 0x2000, 0x1000)
    struct.pack_into('<IIQQQQQQ', image, 176, 0x6474e551, 6, 0, 0, 0, 0, 0, 16)
    struct.pack_into('<IIQQQQQQ', image, 232, 1, 6, 0x2000, 0x2000, 0x2000, 0x1000, 0x1000, 0x1000)
    strings = b'\0RecordArgs\0libc.prx\0shared\0'
    tags = [(5, 0x2600), (10, len(strings)), (6, 0x2400), (11, 24), (4, 0x2500),
            (7, 0x2700), (8, 24), (9, 24), (1, 12), (12, 0x1010), (0, 0)]
    struct.pack_into('<IIQQQQQQ', image, 120, 2, 6, 0x2000, 0x2000, 0x2000,
                     len(tags) * 16, len(tags) * 16, 8)
    for index, tag in enumerate(tags):
        struct.pack_into('<qQ', image, 0x2000 + index * 16, *tag)
    image[0x2600:0x2600 + len(strings)] = strings
    struct.pack_into('<IBBHQQ', image, 0x2418, 1, 0x12, 0, 0, 0, 0)
    struct.pack_into('<IBBHQQ', image, 0x2430, 21, 0x12, 0, 1, 0x1000, 6)
    struct.pack_into('<II', image, 0x2500, 1, 3)
    struct.pack_into('<QQq', image, 0x2700, 0x2820, (1 << 32) | 6, 0)
    image[0x1000:0x1006] = b'\xb8\x2a\0\0\0\xc3'
    code = b'\x48\x83\xec\x08\xff\x15' + struct.pack('<i', 0x2820 - 0x101a)
    code += b'\x48\x83\xc4\x08\xb8' + struct.pack('<I', RETURNED) + b'\xc3'
    image[0x1010:0x1010 + len(code)] = code
    return image


def no_relocation_module():
    image = args_module()
    for position in range(0x2000, 0x2200, 16):
        tag, _ = struct.unpack_from('<qQ', image, position)
        if tag == 0:
            break
        if tag == 8:
            struct.pack_into('<Q', image, position + 8, 0)
    return image


def macho_binds(image):
    linkedit = MachO(image).segment('__LINKEDIT')
    return image[linkedit['fileoff']:linkedit['fileoff'] + linkedit['filesize']]


def load_and_record(work, provider, module):
    events = work / 'events.txt'
    run = subprocess.run([sys.executable, __file__, '--load', str(provider), str(module)],
                         env={**os.environ, 'ANYPS5_GUEST_MODULE_ARGS': str(events)},
                         capture_output=True, text=True, timeout=30)
    assert run.returncode == 0, (run.returncode, run.stdout, run.stderr)
    recorded = events.read_text()
    assert f'args={ARGS:016x} argp={ARGP:016x}' in recorded, recorded
    assert f'result={RETURNED}' in recorded, recorded


def load_and_record_default(work, provider, module):
    events = work / 'events_default.txt'
    run = subprocess.run([sys.executable, __file__, '--load-default', str(provider), str(module)],
                         env={**os.environ, 'ANYPS5_GUEST_MODULE_ARGS': str(events)},
                         capture_output=True, text=True, timeout=30)
    assert run.returncode == 0, (run.returncode, run.stdout, run.stderr)
    recorded = events.read_text()
    assert 'args=0000000000000010' in recorded, recorded
    assert 'argp=0000000000000000' not in recorded, recorded
    assert f'result={RETURNED}' in recorded, recorded


def main():
    if sys.argv[1] == '--load':
        import ctypes
        host = ctypes.CDLL(sys.argv[2], mode=ctypes.RTLD_GLOBAL if sys.platform == 'darwin' else ctypes.DEFAULT_MODE)
        host.SetPendingArgs.argtypes = [ctypes.c_size_t, ctypes.c_void_p]
        host.SetPendingArgs(ARGS, ARGP)
        ctypes.CDLL(sys.argv[3])
        return
    if sys.argv[1] == '--load-default':
        import ctypes
        ctypes.CDLL(sys.argv[2], mode=ctypes.RTLD_GLOBAL if sys.platform == 'darwin' else ctypes.DEFAULT_MODE)
        ctypes.CDLL(sys.argv[3])
        return
    relinker = Path(sys.argv[1]).resolve()
    host = Path(sys.argv[2]).resolve()
    linux = sys.platform == 'linux' and os.uname().machine == 'x86_64'
    macos = sys.platform == 'darwin' and platform.machine() == 'x86_64'
    with tempfile.TemporaryDirectory(prefix='anyps5-guest-args-') as directory:
        work = Path(directory)
        modules = work / 'sce_module'
        modules.mkdir()
        name = 'args.prx'
        (modules / name).write_bytes(args_module())
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
        bare = work / 'bare'
        bare_modules = bare / 'sce_module'
        bare_modules.mkdir(parents=True)
        (bare_modules / name).write_bytes(no_relocation_module())
        bare_source = bare / 'input.elf'
        bare_source.write_bytes(executable(name))
        bare_result = subprocess.run([str(relinker), '--skip-syscall-check', str(bare_source), str(bare / 'output.elf')],
                                     capture_output=True, text=True, timeout=30)
        assert bare_result.returncode == 0, (bare_result.stdout, bare_result.stderr)
        bare_image = (bare / 'app0' / 'sce_module' / (name + '.guest.prx')).read_bytes()
        bare_imports = imported_symbols(bare_image)
        assert GETTER in bare_imports, bare_imports
        assert SETTER in bare_imports, bare_imports
        assert dynamic_tags(bare_image).get(8) == 48, dynamic_tags(bare_image)
        mach = work / 'macos'
        mach_modules = mach / 'sce_module'
        mach_modules.mkdir(parents=True)
        (mach_modules / name).write_bytes(args_module())
        mach_source = mach / 'input.elf'
        mach_source.write_bytes(executable(name))
        mach_result = subprocess.run([str(relinker), '--macos', '--skip-syscall-check', str(mach_source), str(mach / 'output')],
                                     capture_output=True, text=True, timeout=30)
        assert mach_result.returncode == 0, (mach_result.stdout, mach_result.stderr)
        mach_converted = mach / 'app0' / 'sce_module' / (name + '.guest.prx')
        binds = macho_binds(mach_converted.read_bytes())
        assert ('_' + GETTER).encode() in binds, f'the macOS module does not bind {GETTER}'
        assert ('_' + SETTER).encode() in binds, f'the macOS module does not bind {SETTER}'
        if linux:
            libraries = work / 'libs'
            libraries.mkdir()
            shutil.copyfile(host, libraries / 'libc.prx')
            load_and_record(work, libraries / 'libc.prx', converted)
            load_and_record_default(work, libraries / 'libc.prx', converted)
        elif macos:
            libraries = mach / 'libs'
            libraries.mkdir()
            shutil.copyfile(host, libraries / 'libkernel.prx')
            (libraries / 'libc.prx').symlink_to('libkernel.prx')
            load_and_record(mach, libraries / 'libkernel.prx', mach_converted)
            load_and_record_default(mach, libraries / 'libkernel.prx', mach_converted)
        else:
            print('Guest module argument tests passed: native execution skipped')
            return
        print('Guest module argument tests passed')


if __name__ == '__main__':
    main()
