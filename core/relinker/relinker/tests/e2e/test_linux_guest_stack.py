from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture


GNU_STACK = 0x6474E551


def fixture(stack_flags):
    image = bytearray(0x3000)
    image[:16] = b'\x7fELF\x02\x01\x01' + bytes(9)
    struct.pack_into('<HHIQQQIHHHHHH', image, 16,
                     3, 62, 1, 0, 64, 0, 0, 64, 56, 2 if stack_flags is None else 3, 0, 0, 0)
    struct.pack_into('<IIQQQQQQ', image, 64, 1, 7, 0, 0, 0, len(image), len(image), 0x1000)
    tags = [(5, 0x2600), (10, 8), (6, 0x2400), (11, 24), (4, 0x2500), (0, 0)]
    struct.pack_into('<IIQQQQQQ', image, 120, 2, 6, 0x2000, 0x2000, 0x2000,
                     len(tags) * 16, len(tags) * 16, 8)
    if stack_flags is not None:
        struct.pack_into('<IIQQQQQQ', image, 176, GNU_STACK, stack_flags, 0, 0, 0, 0, 0x20000, 16)
    for index, tag in enumerate(tags):
        struct.pack_into('<qQ', image, 0x2000 + index * 16, *tag)
    image[0x2600:0x2608] = b'\0Answer\0'
    struct.pack_into('<IBBHQQ', image, 0x2418, 1, 0x12, 0, 1, 0x1000, 6)
    struct.pack_into('<II', image, 0x2500, 1, 2)
    image[0x1000:0x1006] = b'\xb8\x2a\0\0\0\xc3'
    return image


def stack_headers(image):
    offset, = struct.unpack_from('<Q', image, 32)
    size, count = struct.unpack_from('<HH', image, 54)
    return [header for index in range(count)
            for header in [struct.unpack_from('<IIQQQQQQ', image, offset + index * size)]
            if header[0] == GNU_STACK]


def load(path):
    import ctypes
    import _ctypes
    before = next(line for line in Path('/proc/self/maps').read_text().splitlines() if line.endswith('[stack]'))
    assert 'x' not in before.split()[1], before
    library = ctypes.CDLL(path)
    answer = getattr(library, 'Answer#guest')
    answer.restype = ctypes.c_int
    assert answer() == 42
    after = next(line for line in Path('/proc/self/maps').read_text().splitlines() if line.endswith('[stack]'))
    assert 'x' not in after.split()[1], after
    _ctypes.dlclose(library._handle)


def main():
    if sys.argv[1] == '--load':
        load(sys.argv[2])
        return
    relinker = Path(sys.argv[1]).resolve()
    native = sys.platform == 'linux' and os.uname().machine == 'x86_64'
    with tempfile.TemporaryDirectory(prefix='anyps5-guest-stack-') as directory:
        for flags in (None, 6, 7):
            case = Path(directory) / str(flags)
            modules = case / 'sce_module'
            modules.mkdir(parents=True)
            original = fixture(flags)
            (modules / 'provider.prx').write_bytes(original)
            source = case / 'input.elf'
            source.write_bytes(main_fixture())
            result = subprocess.run([str(relinker), str(source), str(case / 'output.elf')],
                                    capture_output=True, text=True, timeout=30)
            assert result.returncode == 0, (result.stdout, result.stderr)
            converted = case / 'app0' / 'sce_module' / 'provider.prx.guest.prx'
            if native and flags != 7:
                run = subprocess.run([sys.executable, __file__, '--load', str(converted)],
                                     capture_output=True, text=True, timeout=30)
                assert run.returncode == 0, (flags, run.returncode, run.stdout, run.stderr)
            headers = stack_headers(converted.read_bytes())
            assert len(headers) == 1, (flags, headers)
            if flags is None:
                assert headers[0] == (GNU_STACK, 6, 0, 0, 0, 0, 0, 16), headers
            else:
                assert headers == stack_headers(original), (flags, headers)
    print('Linux guest stack conversion tests passed')
    if not native:
        print('Native dlopen execution skipped: Linux x86-64 required')


if __name__ == '__main__':
    main()
