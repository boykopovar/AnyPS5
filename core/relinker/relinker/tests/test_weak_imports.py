"""Check that weak undefined imports stay weak on Linux and macOS and bind to null on Windows when no module exports them."""

import os
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

from test_macos_output import MachO
from test_windows_import_modules import executable, provider
from test_guest_intel_trampolines import main_fixture
from test_guest_module_directories import needed_libraries
from test_guest_symbol_names import dynamic_tags, file_offset
from import_fixture import fixture as macos_fixture
from nid import nid

SHT_DYNSYM = 11
STB_GLOBAL = 1
STB_WEAK = 2
OPTIONAL = 'AAAAAAAAAAA'
REQUIRED = 'BBBBBBBBBBB'
ENTRY = 'ProbeOptional'
HOST = 'libWeakHost.prx'


def module_fixture(symbols, code, dependency=None, relocations=()):
    image = bytearray(0x3000)
    image[:16] = b'\x7fELF\x02\x01\x01' + bytes(9)
    struct.pack_into('<HHIQQQIHHHHHH', image, 16,
                     3, 62, 1, 0, 64, 0, 0, 64, 56, 4, 0, 0, 0)
    struct.pack_into('<IIQQQQQQ', image, 64, 1, 5, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000)
    struct.pack_into('<IIQQQQQQ', image, 120, 1, 6, 0x2000, 0x2000, 0x2000, 0x1000, 0x1000, 0x1000)
    strings = bytearray(b'\0')
    for index, (name, info, section, value, size) in enumerate(symbols, 1):
        struct.pack_into('<IBBHQQ', image, 0x2400 + index * 24,
                         len(strings), info, 0, section, value, size)
        strings.extend(name.encode() + b'\0')
    tags = [(5, 0x2600), (6, 0x2400), (11, 24), (4, 0x2500)]
    if dependency:
        tags += [(1, len(strings)), (0x61000045, (1 << 48) | len(strings))]
        strings.extend(dependency.encode() + b'\0')
    tags.append((10, len(strings)))
    if relocations:
        tags += [(7, 0x2700), (8, len(relocations) * 24), (9, 24)]
        for index, (target, symbol) in enumerate(relocations):
            struct.pack_into('<QQq', image, 0x2700 + index * 24, target, (symbol << 32) | 6, 0)
    tags.append((0, 0))
    struct.pack_into('<IIQQQQQQ', image, 176, 2, 6, 0x2000, 0x2000, 0x2000,
                     len(tags) * 16, len(tags) * 16, 8)
    struct.pack_into('<IIQQQQQQ', image, 232, 0x6474e551, 6, 0, 0, 0, 0, 0, 16)
    for index, tag in enumerate(tags):
        struct.pack_into('<qQ', image, 0x2000 + index * 16, *tag)
    image[0x2600:0x2600 + len(strings)] = strings
    count = len(symbols) + 1
    values = [1, count, count - 1, 0, *range(count - 1)]
    struct.pack_into('<' + 'I' * len(values), image, 0x2500, *values)
    image[0x1000:0x2000] = b'\x90' * 0x1000
    image[0x1000:0x1000 + len(code)] = code
    return image


def weak_guest_module(binding):
    code = b'\x48\x8b\x05' + struct.pack('<i', 0x2800 - 0x1007)
    code += b'\x48\x85\xc0\x74\x02\xff\xe0\xff\x25' + struct.pack('<i', 0x2808 - 0x1014)
    symbols = [(OPTIONAL + '#A#B', (binding << 4) | 2, 0, 0, 0),
               (REQUIRED + '#A#B', 0x12, 0, 0, 0), (ENTRY, 0x12, 1, 0x1000, len(code))]
    return module_fixture(symbols, code, HOST, [(0x2800, 1), (0x2808, 2)])


def weak_host_provider(optional, required):
    symbols = []
    for name, present, address in ((OPTIONAL, optional, 0x1000), (REQUIRED, required, 0x1020)):
        if present:
            symbols.extend((alias, 0x12, 1, address, 6) for alias in (name, name + '#A#B'))
    code = b'\xb8' + struct.pack('<I', 77) + b'\xc3' + b'\x90' * 26
    code += b'\xb8' + struct.pack('<I', 42) + b'\xc3'
    return module_fixture(symbols, code)


def module_import_attributes(image):
    tags = dynamic_tags(image)
    strings = file_offset(image, tags[5])
    symbols = file_offset(image, tags[6])
    count, = struct.unpack_from('<I', image, file_offset(image, tags[4]) + 4)
    attributes = {}
    for index in range(1, count):
        name, info, _, section = struct.unpack_from('<IBBH', image, symbols + index * 24)
        if section == 0:
            symbol = image[strings + name:image.index(0, strings + name)].decode()
            attributes[symbol] = (info >> 4, info & 15)
    return attributes


def load_weak_module(path, symbol, libraries):
    return subprocess.run([sys.executable, __file__, '--load-weak-module', str(path), symbol],
                          env={**os.environ, 'LD_LIBRARY_PATH': str(libraries)},
                          capture_output=True, text=True, timeout=10)


def check_guest_weak_imports(relinker, work):
    linux = sys.platform == 'linux' and os.uname().machine == 'x86_64'
    for name, binding, optional, required, expected, missing in (
            ('weak-missing', STB_WEAK, False, True, 42, None),
            ('weak-present', STB_WEAK, True, True, 77, None),
            ('required-missing', STB_WEAK, True, False, None, REQUIRED),
            ('global-missing', STB_GLOBAL, False, True, None, OPTIONAL)):
        case = work / ('guest-' + name)
        modules, libraries = case / 'sce_module', case / 'libs'
        modules.mkdir(parents=True)
        libraries.mkdir()
        raw = modules / 'optional.prx'
        raw.write_bytes(weak_guest_module(binding))
        (libraries / HOST).write_bytes(weak_host_provider(optional, required))
        source, output = case / 'input.elf', case / 'output.elf'
        source.write_bytes(main_fixture())
        result = subprocess.run([str(relinker), str(source), str(output)],
                                capture_output=True, text=True, timeout=30)
        assert result.returncode == 0, (name, result.stdout, result.stderr)
        converted = case / 'app0/sce_module/optional.prx.guest.prx'
        image = converted.read_bytes()
        attributes = module_import_attributes(image)
        assert attributes.get(OPTIONAL) == (binding, 2), (name, attributes)
        assert attributes.get(REQUIRED) == (STB_GLOBAL, 2), (name, attributes)
        assert needed_libraries(image) == [HOST], (name, needed_libraries(image))
        if linux:
            for module, symbol in ((raw, ENTRY), (converted, ENTRY + '#guest')):
                run = load_weak_module(module, symbol, libraries)
                if expected is None:
                    assert run.returncode != 0 and 'undefined symbol: ' + missing in run.stderr, \
                        (name, module, run.returncode, run.stdout, run.stderr)
                else:
                    assert run.returncode == 0 and run.stdout.strip() == str(expected), \
                        (name, module, run.returncode, run.stdout, run.stderr)
            if name == 'weak-missing':
                (libraries / HOST).unlink()
                run = load_weak_module(converted, ENTRY + '#guest', libraries)
                assert run.returncode != 0 and HOST in run.stderr and 'cannot open shared object file' in run.stderr, \
                    (name, run.returncode, run.stdout, run.stderr)


def weak_executable(owner, symbol):
    image = executable(owner, symbol)
    image[0x4898 + 4] = (STB_WEAK << 4) | 2
    code = (b'\x48\x8b\x05' + struct.pack('<i', 0x4a20 - 0x4007)
            + b'\x48\x85\xc0\x74\x02\xff\xe0\xb8\x21\x00\x00\x00\xc3')
    image[0x4000:0x4000 + len(code)] = code
    return image


def import_bindings(elf):
    shoff, = struct.unpack_from('<Q', elf, 0x28)
    shentsize, shnum, _ = struct.unpack_from('<HHH', elf, 0x3A)
    sections = [struct.unpack_from('<IIQQQQIIQQ', elf, shoff + index * shentsize) for index in range(shnum)]
    dynsym = next(section for section in sections if section[1] == SHT_DYNSYM)
    strings = sections[dynsym[6]][4]
    bindings = {}
    for offset in range(dynsym[4] + 24, dynsym[4] + dynsym[5], 24):
        name, info, _, index = struct.unpack_from('<IBBH', elf, offset)
        if index == 0:
            bindings[elf[strings + name:elf.index(b'\0', strings + name)].decode()] = info >> 4
    return bindings


def macho_bind_opcodes(data):
    macho = MachO(data)
    bind_offset, bind_size = macho.dyld_info[2], macho.dyld_info[3]
    stream = data[bind_offset:bind_offset + bind_size]
    opcodes = {}
    for symbol in ('puts', 'exit'):
        name = ('_' + nid(symbol)).encode() + b'\0'
        at = stream.find(name)
        assert at > 0, (symbol, stream)
        opcodes[symbol] = stream[at - 1]
    return opcodes


def main():
    if sys.argv[1] == '--load-weak-module':
        import ctypes
        try:
            module = ctypes.CDLL(sys.argv[2], mode=os.RTLD_NOW | os.RTLD_LOCAL)
            entry = getattr(module, sys.argv[3])
            entry.restype = ctypes.c_int
            print(entry())
        except OSError as error:
            print(error, file=sys.stderr)
            raise SystemExit(1)
        return
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='anyps5-weak-imports-') as directory:
        work = Path(directory)

        for name, image, symbol, expected in (('linux-weak', weak_executable('a.prx', 'absent#A#B'), 'absent', STB_WEAK),
                                              ('linux-global', executable('a.prx', 'absent#A#B'), 'absent', STB_GLOBAL)):
            source = work / (name + '.elf')
            source.write_bytes(image)
            output = work / (name + '.out')
            result = subprocess.run([str(relinker), '--skip-sce-module', str(source), str(output)],
                                    capture_output=True, text=True, timeout=30)
            assert result.returncode == 0, (name, result.stdout, result.stderr)
            bindings = import_bindings(output.read_bytes())
            assert bindings.get(symbol) == expected, (name, bindings)

        for name, symbol, expected in (('windows-weak-missing', 'absent#A#B', 33),
                                       ('windows-weak-present', 'shared#A#B', 11)):
            case = work / name
            (case / 'prx').mkdir(parents=True)
            (case / 'prx' / 'a.prx').write_bytes(provider(11))
            source = case / 'input.elf'
            source.write_bytes(weak_executable('a.prx', symbol))
            output = case / 'output.exe'
            result = subprocess.run([str(relinker), '--windows', str(source), str(output)],
                                    capture_output=True, text=True, timeout=30)
            assert result.returncode == 0, (name, result.stdout, result.stderr)
            if os.name == 'nt':
                run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
                assert run.returncode == expected, (name, run.returncode, run.stdout, run.stderr)

        weak_puts = macos_fixture()
        weak_puts[0x5B00 + 24 + 4] = (STB_WEAK << 4) | 2
        for name, image, expected in (('macos-weak', weak_puts, {'puts': 0x41, 'exit': 0x40}),
                                      ('macos-global', macos_fixture(), {'puts': 0x40, 'exit': 0x40})):
            source = work / (name + '.elf')
            source.write_bytes(bytes(image))
            output = work / name
            result = subprocess.run([str(relinker), '--macos', '--skip-sce-module', str(source), str(output)],
                                    capture_output=True, text=True, timeout=30)
            assert result.returncode == 0, (name, result.stdout, result.stderr)
            opcodes = macho_bind_opcodes(output.read_bytes())
            assert opcodes == expected, (name, opcodes)
        check_guest_weak_imports(relinker, work)

    print('Weak import tests passed')


if __name__ == '__main__':
    main()
