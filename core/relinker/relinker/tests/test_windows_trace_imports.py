"""--trace-imports passes each resolved function import through libc.prx's TraceImport_nid_no_patch."""

import os
from pathlib import Path
import subprocess
import sys

from guest_import_fixtures import fixture_directory
from test_windows_import_modules import dynamic, executable, provider


HOOK = (b'\x80\x3a\x73\x75\x0e'
        b'\x41\x80\x38\x61\x75\x08'
        b'\x48\x8d\x05\x05\x00\x00\x00\xc3'
        b'\x48\x89\xc8\xc3'
        b'\xb8\x4d\x00\x00\x00\xc3')


def named_provider(symbol, soname, code):
    image = provider(0, code=code)
    strings = b'\0' + symbol.encode() + b'#A#B\0' + soname.encode() + b'\0'
    image[0x800:0x800 + len(strings)] = strings
    tags = [(5, 0x2200), (10, len(strings)), (6, 0x2280), (11, 24),
            (4, 0x2240), (7, 0x2300), (8, 0), (9, 24), (14, len(symbol) + 6)]
    dynamic(image, 0x600, 176, tags)
    return image


def relink(relinker, source, output, *options):
    return subprocess.run([str(relinker), '--windows', *options, str(source), str(output)],
                          capture_output=True, text=True, timeout=30)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with fixture_directory('anyps5-trace-imports-') as directory:
        work = Path(directory)
        build = work / 'providers'
        (build / 'prx').mkdir(parents=True)
        (build / 'prx' / 'a.prx').write_bytes(provider(11))
        (build / 'prx' / 'hook.prx').write_bytes(named_provider('TraceImport_nid_no_patch', 'hook.prx', HOOK))
        (build / 'prx' / 'plain.prx').write_bytes(named_provider('Unrelated', 'plain.prx', b'\x31\xc0\xc3'))
        source = build / 'input.elf'
        source.write_bytes(executable('a.prx'))
        result = relink(relinker, source, build / 'output.exe')
        assert result.returncode == 0, (result.stdout, result.stderr)
        converted = {name: (build / 'app0' / 'prx' / (name + '.prx.guest.prx')).read_bytes() for name in ('a', 'hook', 'plain')}

        for name, options, libc, symbol_info, expected, message in (
                ('untraced', (), 'hook', 0x12, 11, None),
                ('traced', ('--trace-imports',), 'hook', 0x12, 77, None),
                ('data', ('--trace-imports',), 'hook', 0x11, 11, None),
                ('weak', ('--trace-imports',), 'hook', 0x22, 11, None),
                ('unsupported', ('--trace-imports',), 'plain', 0x12, None, 'libc.prx does not export TraceImport_nid_no_patch')):
            case = work / name
            (case / 'libs').mkdir(parents=True)
            (case / 'prx').mkdir()
            (case / 'libs' / 'a.prx').write_bytes(converted['a'])
            (case / 'libs' / 'libc.prx').write_bytes(converted[libc])
            source = case / 'input.elf'
            image = executable('a.prx', extra_dependencies=('libc.prx',))
            image[0x4898 + 4] = symbol_info
            source.write_bytes(image)
            output = case / 'output.exe'
            result = relink(relinker, source, output, *options)
            assert result.returncode == 0, (name, result.stdout, result.stderr)
            if os.name != 'nt':
                continue
            run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
            if expected is None:
                assert run.returncode != 0 and message in run.stderr, (name, run.returncode, run.stdout, run.stderr)
            else:
                assert run.returncode == expected, (name, run.returncode, run.stdout, run.stderr)

        case = work / 'without-libc'
        (case / 'prx').mkdir(parents=True)
        source = case / 'input.elf'
        source.write_bytes(executable('a.prx'))
        result = relink(relinker, source, case / 'output.exe', '--trace-imports')
        assert result.returncode == 2 and '--trace-imports requires libc.prx' in result.stderr, (result.returncode, result.stderr)
        assert not (case / 'output.exe').exists()

        result = relink(relinker, source, case / 'output.exe', '--trace-imports', '--lazy-binding')
        assert result.returncode != 0 and '--trace-imports conflicts with --lazy-binding' in result.stderr, (result.returncode, result.stderr)

    print('Windows import tracing tests passed')


if __name__ == '__main__':
    main()
