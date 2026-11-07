from pathlib import Path
import os
import platform
import struct
import subprocess
import sys
import tempfile

from test_linux_load_alignment import fixture
from test_macos_output import MachO


ABSOLUTE = 0x123456789ABCDEF
SYMBOLS = (
    ('FixtureExport#A#B', 0x12, 0, 1, 0x210, 6),
    ('FixtureData#A#B', 0x11, 0, 1, 0x280, 4),
    ('FixtureWeak#A#B', 0x22, 0, 1, 0x220, 6),
    ('FixtureAbsolute#A#B', 0x11, 0, 0xFFF1, ABSOLUTE, 0),
    ('FixtureHidden#A#B', 0x12, 2, 1, 0x210, 6),
    ('FixtureLocal#A#B', 0x02, 0, 1, 0x210, 6),
    ('FixtureProtected#A#B', 0x12, 3, 1, 0x210, 6),
)


def executable(symbols=SYMBOLS, tls=False):
    image = fixture()
    struct.pack_into('<Q', image, 24, 0x210)
    image[0x4210:0x4216] = bytes.fromhex('b82a000000c3')
    image[0x4220:0x4226] = bytes.fromhex('b809000000c3')
    struct.pack_into('<I', image, 0x4280, 5)
    strings = bytearray(b'\0')
    for index, (name, info, visibility, section, value, size) in enumerate(symbols, 1):
        offset = len(strings)
        strings.extend(name.encode() + b'\0')
        struct.pack_into('<IBBHQQ', image, 0x4900 + index * 24, offset, info, visibility, section, value, size)
    image[0x4600:0x4600 + len(strings)] = strings
    tags = [(5, 0x600), (10, len(strings)), (6, 0x900), (11, 24), (7, 0x700),
            (8, 0), (9, 24), (0x6100003F, (len(symbols) + 1) * 24), (0, 0)]
    struct.pack_into('<IIQQQQQQ', image, 120, 2, 6, 0x4800, 0x800, 0x800, len(tags) * 16, len(tags) * 16, 8)
    for index, tag in enumerate(tags):
        struct.pack_into('<qQ', image, 0x4800 + index * 16, *tag)
    if tls:
        struct.pack_into('<IIQQQQQQ', image, 176, 7, 4, 0x4300, 0x300, 0x300, 8, 16, 8)
        image[0x4300:0x4308] = bytes(range(8))
    return image


def exports(data):
    macho = MachO(data)
    start, size = macho.dyld_info[8:10]
    trie = data[start:start + size]
    result = {}

    def uleb(offset):
        value, shift = 0, 0
        while True:
            byte = trie[offset]
            offset += 1
            value |= (byte & 0x7F) << shift
            if not byte & 0x80:
                return value, offset
            shift += 7

    def visit(offset, name):
        length, terminal = uleb(offset)
        if length:
            flags, position = uleb(terminal)
            value, _ = uleb(position)
            result[name] = (flags, value)
        position = terminal + length
        children = trie[position]
        position += 1
        for _ in range(children):
            end = trie.index(0, position)
            label = trie[position:end].decode()
            child, position = uleb(end + 1)
            visit(child, name + label)

    if trie:
        visit(0, '')
    return result


def convert(relinker, work, name, image):
    source, output = work / (name + '.elf'), work / name
    source.write_bytes(image)
    run = subprocess.run([str(relinker), '--macos', '--skip-sce-module', str(source), str(output)],
                         capture_output=True, text=True, timeout=10)
    return run, output


def native_execution(work, output):
    if sys.platform != 'darwin' or platform.machine() not in ('arm64', 'x86_64'):
        return
    libraries = work / 'libs'
    libraries.mkdir()
    source = work / 'kernel.c'
    source.write_text(
        '#include <dlfcn.h>\n'
        '#include <stdint.h>\n'
        '#include <unistd.h>\n'
        'extern int FixtureExport(void);\n'
        'extern int FixtureData;\n'
        'void Aps5StartGuest_nid_no_patch(void) {\n'
        '    int (*weak)(void) = dlsym(RTLD_DEFAULT, "FixtureWeak");\n'
        '    int (*protected)(void) = dlsym(RTLD_DEFAULT, "FixtureProtected");\n'
        '    if (!weak || weak() != 9 || !protected || protected() != 42) _exit(1);\n'
        f'    if ((uintptr_t)dlsym(RTLD_DEFAULT, "FixtureAbsolute") != {ABSOLUTE}ULL) _exit(2);\n'
        '    if (dlsym(RTLD_DEFAULT, "FixtureHidden") || dlsym(RTLD_DEFAULT, "FixtureLocal")) _exit(3);\n'
        '    _exit(FixtureExport() + FixtureData - 5);\n'
        '}\n')
    for name in ('libkernel.prx', 'libc.prx'):
        subprocess.run(['xcrun', 'clang', '-arch', 'x86_64', '-dynamiclib', str(source),
                        '-undefined', 'dynamic_lookup', '-Wl,-install_name,@rpath/' + name,
                        '-o', str(libraries / name)], check=True, capture_output=True, timeout=20)
        source.write_text('int FixtureHostLibc(void) { return 0; }\n')
    output.chmod(0o755)
    command = (['/usr/bin/arch', '-x86_64'] if platform.machine() == 'arm64' else []) + [str(output)]
    run = subprocess.run(command, capture_output=True, timeout=10)
    if run.returncode != 42 and b'Bad CPU type in executable' in run.stderr:
        assert os.environ.get('GITHUB_ACTIONS') != 'true', 'macOS CI must execute the x86_64 export probe'
        return
    assert run.returncode == 42, (run.returncode, run.stdout, run.stderr)
    print('macOS executable exports: native x86_64 imports and dlsym passed')


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='anyps5-macos-executable-exports-') as directory:
        work = Path(directory)
        run, output = convert(relinker, work, 'exports', executable())
        assert run.returncode == 0, (run.stdout, run.stderr)
        data = output.read_bytes()
        entries = exports(data)
        base = MachO(data).segment('__ELF0')['vmaddr']
        assert entries == {
            '_FixtureExport': (0, base + 0x210 - 0x140000000),
            '_FixtureData': (0, base + 0x280 - 0x140000000),
            '_FixtureWeak': (4, base + 0x220 - 0x140000000),
            '_FixtureAbsolute': (2, ABSOLUTE),
            '_FixtureProtected': (0, base + 0x210 - 0x140000000),
        }, entries
        assert MachO(data).flags & 0x8000
        native_execution(work, output)
        tls_symbols = (('FixtureTls#A#B', 0x16, 0, 1, 4, 4),)
        for info, flags in ((0x16, 0), (0x26, 4)):
            run, output = convert(relinker, work, f'tls-{info}', executable((('FixtureTls#A#B', info, 0, 1, 4, 4),), tls=True))
            assert run.returncode == 0, (run.stdout, run.stderr)
            data = output.read_bytes()
            entry, = exports(data).values()
            assert entry[0] == flags
            assert bool(MachO(data).flags & 0x8000) == bool(flags)
            meta = MachO(data).segment('__APS5DATA')
            slot = meta['fileoff'] + entry[1] + 0x140000000 - meta['vmaddr']
            assert struct.unpack_from('<QQ', data, slot) == (meta['vmaddr'] + 0x20, 4)
        for name, symbols, tls, error in (
                ('duplicate', SYMBOLS + (SYMBOLS[0],), False, 'Duplicate Mach-O export'),
                ('unmapped', (('Unmapped', 0x12, 0, 1, 0x100000, 6),), False, 'does not fit inside an executable PT_LOAD segment'),
                ('tls-outside', (('Tls', 0x16, 0, 1, 16, 1),), True, 'exceeds PT_TLS'),
                ('tls-size', (('Tls', 0x16, 0, 1, 4, 13),), True, 'exceeds PT_TLS'),
                ('tls-missing', tls_symbols, False, 'has no PT_TLS segment'),
                ('reserved', (('Common', 0x11, 0, 0xFFF2, 0, 4),), False, 'Unsupported executable export section'),
                ('attributes', (('Indirect', 0x1A, 0, 1, 0x210, 6),), False, 'Unsupported executable export attributes: Indirect')):
            run, output = convert(relinker, work, name, executable(symbols, tls))
            assert run.returncode == 2 and error in run.stderr, (name, run.returncode, run.stderr)
            assert not output.exists(), name
    print('macOS executable export tests passed')


if __name__ == '__main__':
    main()
