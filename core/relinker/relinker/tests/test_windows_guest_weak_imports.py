"""Check that a weak import of a bundled module binds to null on Windows when no library exports it."""

import os
import struct
import subprocess
import sys
from pathlib import Path

from guest_import_fixtures import fixture_directory
from test_windows_import_modules import consumer, executable, provider

STB_WEAK = 2


def weak_consumer(owner, symbol):
    image = consumer(owner, symbol)
    image[0x898 + 4] = (STB_WEAK << 4) | 2
    code = (b'\x48\x8b\x05' + struct.pack('<i', 0x2320 - 0x1017)
            + b'\x48\x85\xc0\x74\x02\x0f\x0b\xc3')
    image[0x410:0x410 + len(code)] = code
    return image


def main():
    relinker = Path(sys.argv[1]).resolve()
    with fixture_directory('anyps5-windows-guest-weak-') as directory:
        work = Path(directory)

        for name, image, expected in (('windows-guest-weak-missing', weak_consumer('a.prx', 'absent#A#B'), 11),
                                      ('windows-guest-global-missing', consumer('a.prx', 'absent#A#B'), None)):
            case = work / name
            (case / 'prx').mkdir(parents=True)
            (case / 'prx' / 'a.prx').write_bytes(provider(11))
            (case / 'prx' / 'consumer.prx').write_bytes(image)
            source = case / 'input.elf'
            source.write_bytes(executable('a.prx', 'shared#A#B'))
            output = case / 'output.exe'
            result = subprocess.run([str(relinker), '--windows', str(source), str(output)],
                                    capture_output=True, text=True, timeout=30)
            assert result.returncode == 0, (name, result.stdout, result.stderr)
            if os.name == 'nt':
                run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
                if expected is None:
                    assert run.returncode != 0 and 'unresolved ELF import absent from a.prx' in run.stderr, (name, run.returncode, run.stderr)
                else:
                    assert run.returncode == expected, (name, run.returncode, run.stdout, run.stderr)
    print('Windows guest weak import tests passed')


if __name__ == '__main__':
    main()
