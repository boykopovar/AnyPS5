import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'relinker' / 'relinker' / 'tests'))
from guest_import_fixtures import executable, fixture_directory


def call(code, slot):
    code.extend(b'\xff\x15' + struct.pack('<i', slot - (0x4000 + len(code) + 6)))


def fixture(variant, label):
    image = executable(variant, 'PkS44IGrDkM#A#B', extra_symbols=('OxhIB8LB-PQ#A#B',))
    code = bytearray(b'\x48\x83\xec\x38')
    if label == 'join':
        code.extend(b'\x48\x8d\x3c\x24\x31\xf6')
        code.extend(b'\x48\x8d\x15' + struct.pack('<i', 0x4300 - (0x4000 + len(code) + 7)))
        code.extend(b'\x48\xb9' + struct.pack('<Q', 0x123456789abcdef))
        call(code, 0x4a28)
        code.extend(b'\x85\xc0\x0f\x85')
        create_failure = len(code)
        code.extend(bytes(4))
        code.extend(b'\x48\x8b\x3c\x24\x48\x8d\x74\x24\x08')
        code.extend(b'\x48\xb8' + struct.pack('<Q', 0x7fffffffffffffff))
        code.extend(b'\x48\x89\x44\x24\x10')
        code.extend(b'\x48\xc7\x44\x24\x18' + struct.pack('<I', 999999999))
        code.extend(b'\x48\x8d\x54\x24\x10')
        call(code, 0x4a20)
        code.extend(b'\x85\xc0\x0f\x85')
        join_failure = len(code)
        code.extend(bytes(4))
        code.extend(b'\x48\xba' + struct.pack('<Q', 0x123456789abcdef))
        code.extend(b'\x48\x39\x54\x24\x08\x0f\x85')
        value_failure = len(code)
        code.extend(bytes(4))
        code.extend(b'\x31\xc0\x48\x83\xc4\x38\xc3')
        failure = len(code)
        code.extend(b'\xb8\x63\x00\x00\x00\x48\x83\xc4\x38\xc3')
        for offset in (create_failure, join_failure, value_failure):
            struct.pack_into('<i', code, offset, failure - offset - 4)
        image[0x4300:0x4304] = b'\x48\x89\xf8\xc3'
    else:
        thread = 1 if label == 'invalid-deadline' else 0
        code.extend(b'\xbf' + struct.pack('<I', thread) + b'\x31\xf6\x31\xd2')
        if label != 'null-deadline':
            code.extend(b'\x48\xc7\x44\x24\x10' + struct.pack('<i', -1 if thread else 0))
            code.extend(b'\x48\xc7\x44\x24\x18\x00\x00\x00\x00')
            code.extend(b'\x48\x8d\x54\x24\x10')
        call(code, 0x4a20)
        code.extend(b'\x48\x83\xc4\x38\xc3')
    image[0x4000:0x4000 + len(code)] = code
    return image


def main():
    assert os.name == 'nt'
    relinker, libraries = (Path(value).resolve() for value in sys.argv[1:3])
    variant = sys.argv[3]
    runtime = Path(sys.argv[4]).resolve()
    assert variant in ('libkernel_sys.prx', 'libkernel_web.prx')
    with fixture_directory('anyps5-timed-join-') as directory:
        work = Path(directory)
        (work / 'libs').mkdir()
        (work / 'prx').mkdir()
        for name in (variant, 'libkernel.prx', 'libc.prx'):
            shutil.copy2(libraries / name, work / 'libs' / name)
        for name in ('libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll'):
            shutil.copy2(runtime / name, work / 'libs' / name)
        for label, expected in (('null-deadline', 22), ('invalid-deadline', 22), ('null-thread', 22), ('join', 0)):
            source = work / 'input.elf'
            source.write_bytes(fixture(variant, label))
            output = work / (label + '.exe')
            result = subprocess.run([str(relinker), '--windows', str(source), str(output)],
                                    capture_output=True, text=True, timeout=10)
            assert result.returncode == 0, (variant, label, result.stderr)
            result = subprocess.run([str(output)], cwd=work, capture_output=True, text=True, timeout=10)
            assert result.returncode == expected, (variant, label, result.returncode, result.stderr)
    print(variant + ': scoped timed-join validation and shared thread lifecycle passed')


if __name__ == '__main__':
    main()
