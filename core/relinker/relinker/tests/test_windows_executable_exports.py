from pathlib import Path
import ctypes
import os
import struct
import subprocess
import sys
import tempfile

from guest_import_fixtures import dynamic
from test_guest_intel_trampolines import main_fixture, pe_sections


def fixture(symbols, tls=False):
    image = main_fixture()
    image[0x4020:0x4026] = b'\xb8\x2b\0\0\0\xc3'
    struct.pack_into('<Q', image, 0x4f00, 0x123456789abcdef0)
    strings = bytearray(b'\0')
    table = bytearray(24)
    for name, info, visibility, section, value, size in symbols:
        offset = len(strings)
        strings.extend(name.encode() + b'\0')
        table.extend(struct.pack('<IBBHQQ', offset, info, visibility, section, value, size))
    image[0x4800:0x4800 + len(strings)] = strings
    image[0x4a00:0x4a00 + len(table)] = table
    struct.pack_into('<II', image, 0x4e00, 1, len(table) // 24)
    tags = [(5, 0x4800), (10, len(strings)), (6, 0x4a00), (11, 24),
            (4, 0x4e00), (0x6100003f, len(table)), (7, 0x4700), (8, 0), (9, 24)]
    dynamic(image, 0x4600, 120, tags)
    if tls:
        struct.pack_into('<IIQQQQQQ', image, 176, 7, 4, 0x4f80, 0x4f80, 0x4f80, 8, 64, 16)
    return image


def exports(pe):
    nt = struct.unpack_from('<I', pe, 0x3c)[0]
    rva, size = struct.unpack_from('<II', pe, nt + 24 + 112)
    if not rva:
        return {}
    def offset(rva):
        for _, start, raw_size, raw, _ in pe_sections(pe):
            if start <= rva < start + raw_size:
                return raw + rva - start
        raise AssertionError(hex(rva))
    directory = offset(rva)
    count, functions, names, ordinals = struct.unpack_from('<IIII', pe, directory + 24)
    result = {}
    for index in range(count):
        name = offset(struct.unpack_from('<I', pe, offset(names) + index * 4)[0])
        name = pe[name:pe.index(b'\0', name)].decode()
        ordinal = struct.unpack_from('<H', pe, offset(ordinals) + index * 2)[0]
        result[name] = struct.unpack_from('<I', pe, offset(functions) + ordinal * 4)[0]
        assert not rva <= result[name] < rva + size, 'ordinary export became a forwarder'
    assert list(result) == sorted(result), 'export name pointers are not sorted'
    return result


def native_lookup(output):
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.LoadLibraryExW.argtypes = [ctypes.c_wchar_p, ctypes.c_void_p, ctypes.c_uint32]
    kernel.LoadLibraryExW.restype = ctypes.c_void_p
    kernel.GetProcAddress.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    kernel.GetProcAddress.restype = ctypes.c_void_p
    kernel.FreeLibrary.argtypes = [ctypes.c_void_p]
    module = kernel.LoadLibraryExW(str(output), None, 1)
    assert module, ctypes.get_last_error()
    try:
        for name, value in ((b'alpha', 42), (b'weak', 43), (b'zeta', 42)):
            function = kernel.GetProcAddress(module, name)
            assert function, name
            assert ctypes.WINFUNCTYPE(ctypes.c_int)(function)() == value, name
        data = kernel.GetProcAddress(module, b'data')
        assert data and ctypes.c_uint64.from_address(data).value == 0x123456789abcdef0
        assert not kernel.GetProcAddress(module, b'hidden')
    finally:
        assert kernel.FreeLibrary(module)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='anyps5-main-exports-') as directory:
        work = Path(directory)
        def convert(label, symbols, tls=False):
            source = work / (label + '.elf')
            output = work / (label + '.exe')
            source.write_bytes(fixture(symbols, tls))
            result = subprocess.run([str(relinker), '--windows', '--skip-sce-module',
                                     str(source), str(output)], capture_output=True, text=True, timeout=10)
            return result, output
        symbols = [('zeta', 0x12, 0, 1, 0x4000, 6),
                   ('alpha#A#B', 0x12, 0, 1, 0x4000, 6),
                   ('alpha#B#B', 0x12, 0, 1, 0x4000, 6),
                   ('weak', 0x22, 0, 1, 0x4020, 6),
                   ('data', 0x11, 0, 1, 0x4f00, 8),
                   ('protected', 0x12, 3, 1, 0x4000, 6),
                   ('hidden', 0x12, 2, 1, 0x4000, 6),
                   ('internal', 0x12, 1, 1, 0x4000, 6),
                   ('local', 0x02, 0, 1, 0x4000, 6),
                   ('absolute', 0x10, 0, 0xfff1, 1, 0)]
        result, output = convert('lookup', symbols)
        assert result.returncode == 0, (result.stdout, result.stderr)
        actual = exports(output.read_bytes())
        assert actual == {'alpha': 0x10000, 'data': 0x10f00,
                                                'protected': 0x10000, 'weak': 0x10020, 'zeta': 0x10000}, actual
        if os.name == 'nt':
            native_lookup(output)
        for label, changed, expected in (
                ('conflict', symbols[:2] + [('alpha#C#B', 0x12, 0, 1, 0x4020, 6)], 'Conflicting executable exports after stripping #: alpha'),
                ('unmapped', [('bad', 0x11, 0, 1, 0x9000, 6)], 'does not fit inside a PT_LOAD segment'),
                ('reserved', [('bad', 0x11, 0, 0xfff2, 42, 1)], 'Unsupported executable export section'),
                ('empty', [('', 0x12, 0, 1, 0x4000, 6)], 'Empty executable export'),
                ('ifunc', [('bad', 0x1a, 0, 1, 0x4000, 6)], 'Unsupported executable export attributes: bad')):
            result, _ = convert(label, changed)
            assert result.returncode != 0 and expected in result.stderr, (label, result.stdout, result.stderr)
        result, output = convert('tls', [('tls_value', 0x16, 0, 1, 8, 8)], tls=True)
        assert result.returncode == 0, (result.stdout, result.stderr)
        pe = output.read_bytes()
        descriptor = exports(pe)['tls_value']
        location = next(raw + descriptor - start for _, start, size, raw, _ in pe_sections(pe) if start <= descriptor < start + size)
        index, offset = struct.unpack_from('<QQ', pe, location)
        nt = struct.unpack_from('<I', pe, 0x3c)[0]
        tls_rva = struct.unpack_from('<I', pe, nt + 24 + 112 + 9 * 8)[0]
        tls_location = next(raw + tls_rva - start for _, start, size, raw, _ in pe_sections(pe)
                            if start <= tls_rva < start + size)
        expected_index = struct.unpack_from('<Q', pe, tls_location + 16)[0]
        assert offset == 8 and index == expected_index
        for label, value, size, tls, expected in (
                ('tls-missing', 0, 8, False, 'has no PT_TLS segment'),
                ('tls-end', 64, 1, True, 'exceeds PT_TLS'),
                ('tls-range', 60, 8, True, 'exceeds PT_TLS')):
            result, _ = convert(label, [('tls_value', 0x16, 0, 1, value, size)], tls=tls)
            assert result.returncode != 0 and expected in result.stderr, (label, result.stdout, result.stderr)
        result, output = convert('no-exports', [])
        assert result.returncode == 0 and exports(output.read_bytes()) == {}, (result.stdout, result.stderr)
    print('Windows executable export lookup, calls, data and rejection cases passed')


if __name__ == '__main__':
    main()
