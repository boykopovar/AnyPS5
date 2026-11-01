from pathlib import Path
import os
import shutil
import struct
import subprocess
import sys
import tempfile

from test_guest_module_args import ARGS, ARGP, args_module
from test_guest_symbol_names import dynamic_tags
from test_windows_import_modules import executable


def fini_module(with_init):
    image = args_module()
    tags = dynamic_tags(image)
    name = tags[10]
    image[0x2600 + name:0x2600 + name + 11] = b'RecordFini\0'
    entries = [(tag, value + 11 if tag == 10 else 48 if tag == 8 else value)
               for tag, value in tags.items() if with_init or tag != 12]
    entries += [(13, 0x1040), (0, 0)]
    for index, entry in enumerate(entries):
        struct.pack_into('<qQ', image, 0x2000 + index * 16, *entry)
    struct.pack_into('<QQ', image, 120 + 32, len(entries) * 16, len(entries) * 16)
    struct.pack_into('<IBBHQQ', image, 0x2448, name, 0x12, 0, 0, 0, 0)
    struct.pack_into('<II', image, 0x2500, 1, 4)
    struct.pack_into('<QQq', image, 0x2718, 0x2828, (3 << 32) | 6, 0)
    image[0x1040:0x1046] = b'\xff\x25' + struct.pack('<i', 0x2828 - 0x1046)
    return image


def main():
    if sys.argv[1] == '--load':
        import ctypes
        import _ctypes
        host = ctypes.CDLL(sys.argv[2])
        host.SetPendingArgs.argtypes = [ctypes.c_size_t, ctypes.c_void_p]
        host.SetPendingArgs(ARGS, ARGP)
        module = ctypes.CDLL(sys.argv[3])
        shared = getattr(module, 'shared#guest')
        base = ctypes.cast(shared, ctypes.c_void_p).value - 0x1000
        fini = dynamic_tags(Path(sys.argv[3]).read_bytes())[13]
        callback = ctypes.CFUNCTYPE(None, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_void_p)(base + fini)
        callback(ARGS, ARGP, 0x12345678)
        _ctypes.dlclose(module._handle)
        return
    relinker = Path(sys.argv[1]).resolve()
    host = Path(sys.argv[2]).resolve()
    native = sys.platform == 'linux' and os.uname().machine == 'x86_64'
    with tempfile.TemporaryDirectory(prefix='anyps5-guest-fini-') as directory:
        for with_init in (False, True):
            work = Path(directory) / str(with_init)
            modules = work / 'sce_module'
            modules.mkdir(parents=True)
            (modules / 'fini.prx').write_bytes(fini_module(with_init))
            source = work / 'input.elf'
            source.write_bytes(executable('fini.prx'))
            result = subprocess.run([str(relinker), '--skip-syscall-check', str(source), str(work / 'output.elf')],
                                    capture_output=True, text=True, timeout=30)
            assert result.returncode == 0, (result.stdout, result.stderr)
            converted = work / 'app0' / 'sce_module' / 'fini.prx.guest.prx'
            assert dynamic_tags(converted.read_bytes()).get(13, 0) != 0
            if not native:
                continue
            libraries = work / 'libs'
            libraries.mkdir()
            provider = libraries / 'libc.prx'
            shutil.copyfile(host, provider)
            shutil.copyfile(host, libraries / 'libkernel.prx')
            events = work / 'events.txt'
            run = subprocess.run([sys.executable, __file__, '--load', str(provider), str(converted)],
                                 env={**os.environ, 'ANYPS5_GUEST_MODULE_ARGS': str(events)},
                                 capture_output=True, text=True, timeout=30)
            assert run.returncode == 0, (run.returncode, run.stdout, run.stderr)
            recorded = [line for line in events.read_text().splitlines() if line.startswith('fini=')]
            expected = 'fini=0000000000000000 argp=0000000000000000 callback=0000000000000000'
            assert recorded == [expected, expected], (with_init, recorded)
    print('Guest module finalizer tests passed' + ('' if native else ': native execution skipped'))


if __name__ == '__main__':
    main()
