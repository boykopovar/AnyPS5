from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import PLAIN_SITE, guest_fixture, main_fixture
from test_guest_module_directories import needed_libraries
from test_guest_symbol_names import imported_symbols, module_symbols
from test_windows_import_modules import executable


def guest_symbols(symbols, dependency=None):
    image = guest_fixture(PLAIN_SITE)
    image[0x400:0x406] = b'\xb8\x2a\x00\x00\x00\xc3'
    strings = bytearray(b'\0')
    table = bytearray(24)
    for name, info, visibility, section in symbols:
        offset = len(strings)
        strings.extend(name.encode() + b'\0')
        table.extend(struct.pack('<IBBHQQ', offset, info, visibility, section,
                                 0x1000 if section else 0, 6 if section else 0))
    dependency_offset = len(strings)
    if dependency:
        strings.extend(dependency.encode() + b'\0')
    string_offset = 0x800
    symbol_offset = (string_offset + len(strings) + 7) & ~7
    hash_offset = symbol_offset + len(table)
    end = hash_offset + 8 + 4 * (1 + len(table) // 24)
    image.extend(bytes(max(0, end - len(image))))
    image[string_offset:string_offset + len(strings)] = strings
    image[symbol_offset:symbol_offset + len(table)] = table
    image[hash_offset:end] = bytes(end - hash_offset)
    struct.pack_into('<II', image, hash_offset, 1, len(table) // 24)
    address = lambda offset: offset + 0x1a00
    tags = [(5, address(string_offset)), (10, len(strings)),
            (6, address(symbol_offset)), (11, 24), (4, address(hash_offset))]
    if dependency:
        tags.append((1, dependency_offset))
    tags.append((0, 0))
    image[0x600:0x800] = bytes(0x200)
    for index, tag in enumerate(tags):
        struct.pack_into('<qQ', image, 0x600 + index * 16, *tag)
    struct.pack_into('<QQ', image, 120 + 32, end - 0x600, end - 0x600)
    struct.pack_into('<QQ', image, 176 + 32, len(tags) * 16, len(tags) * 16)
    return image


def large_modules(count):
    names = [f'entry{index:06d}' for index in range(count)]
    return {
        'provider.prx': guest_symbols((name, 0x12, 0, 1) for name in names),
        'consumer.prx': guest_symbols(((name, 0x12, 0, 0) for name in reversed(names)),
                                      'provider.prx'),
    }


def convert(relinker, case, windows, modules, source=None):
    directory = case / 'sce_module'
    directory.mkdir(parents=True)
    for name, image in modules.items():
        (directory / name).write_bytes(image)
    input_path = case / 'input.elf'
    input_path.write_bytes(main_fixture() if source is None else source)
    output = case / ('output.exe' if windows else 'output.elf')
    result = subprocess.run([str(relinker), *(['--windows'] if windows else []),
                             str(input_path), str(output)], capture_output=True,
                            text=True, timeout=30)
    return result, output


def main():
    relinker = Path(sys.argv[1]).resolve()
    cases = 0
    with tempfile.TemporaryDirectory(prefix='anyps5-export-lookup-') as directory:
        work = Path(directory)
        for windows in (False, True):
            modules = large_modules(4096)
            result, output = convert(relinker, work / f'large-{windows}', windows, modules,
                                     executable('provider.prx', symbol='entry004095'))
            assert result.returncode == 0, (result.stdout, result.stderr)
            provider = output.parent / 'app0/sce_module/provider.prx.guest.prx'
            consumer = output.parent / 'app0/sce_module/consumer.prx.guest.prx'
            assert provider.is_file() and consumer.is_file()
            if not windows:
                names = {f'entry{index:06d}#guest' for index in range(4096)}
                assert set(module_symbols(provider.read_bytes())) == names
                assert set(module_symbols(consumer.read_bytes())) == names
                assert needed_libraries(consumer.read_bytes()) == ['$ORIGIN/provider.prx.guest.prx']
                assert imported_symbols(output.read_bytes()) == {'entry004095#guest'}
            elif os.name == 'nt':
                run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
                assert run.returncode == 42, (run.returncode, run.stdout, run.stderr)
            cases += 1
            for label, exports, imports, error in (
                    ('object', [('entry', 0x11, 0, 1)], [('entry', 0x11, 0, 0)], None),
                    ('unspecified', [('entry', 0x12, 0, 1)], [('entry', 0x10, 0, 0)], None),
                    ('mismatch', [('entry', 0x11, 0, 1)], [('entry', 0x12, 0, 0)],
                     'Guest import/export type mismatch: entry'),
                    ('filtered-first', [('entry', 0x12, 2, 1), ('entry', 0x11, 0, 1)],
                     [('entry', 0x11, 0, 0)], None),
                    ('filtered-mismatch', [('entry', 0x12, 2, 1), ('entry', 0x11, 0, 1)],
                     [('entry', 0x12, 0, 0)], 'Guest import/export type mismatch: entry'),
                    ('internal-first', [('entry', 0x12, 1, 1), ('entry', 0x11, 0, 1)],
                     [('entry', 0x11, 0, 0)], None),
                    ('local-first', [('entry', 0x02, 0, 1), ('entry', 0x11, 0, 1)],
                     [('entry', 0x11, 0, 0)], None),
                    ('duplicate-first', [('entry#A#A', 0x12, 0, 1), ('entry#B#A', 0x12, 0, 1)],
                     [('entry', 0x12, 0, 0)], None if windows else 'Duplicate or empty export after stripping #: entry'),
                    ('duplicate-mismatch', [('entry#A#A', 0x12, 0, 1), ('entry#B#A', 0x12, 0, 1)],
                     [('entry', 0x11, 0, 0)], 'Guest import/export type mismatch: entry' if windows else 'Duplicate or empty export after stripping #: entry'),
                    ('weak', [('entry', 0x22, 0, 1)], [('entry', 0x12, 0, 0)], None),
                    ('protected', [('entry', 0x12, 3, 1)], [('entry', 0x12, 0, 0)], None)):
                result, _ = convert(relinker, work / f'{label}-{windows}', windows, {
                    'provider.prx': guest_symbols(exports),
                    'consumer.prx': guest_symbols(imports, 'provider.prx'),
                })
                if error:
                    assert result.returncode != 0 and error in result.stderr, (result.stdout, result.stderr)
                else:
                    assert result.returncode == 0, (result.stdout, result.stderr)
                cases += 1
    print(f'{cases} guest export lookup cases passed')


if __name__ == '__main__':
    main()
