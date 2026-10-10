"""Check nid_patcher on small byte-built Mach-O dylibs: the export trie, the chained imports and what it refuses."""

import hashlib
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

SUFFIX = bytes([0x51, 0x8D, 0x64, 0xA6, 0x35, 0xDE, 0xD8, 0xC1, 0xE6, 0xB0, 0x39, 0xB1, 0xC3, 0xE5, 0x52, 0x30])
BASE64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-"

LC_SEGMENT_64 = 0x19
LC_LOAD_DYLIB = 0x0C
LC_CODE_SIGNATURE = 0x1D
LC_DYLD_INFO_ONLY = 0x80000022
LC_DYLD_EXPORTS_TRIE = 0x80000033
LC_DYLD_CHAINED_FIXUPS = 0x80000034
EXPORT_WEAK = 0x04
PAGE = 0x1000


def nid(name):
    digest = hashlib.sha1(name.encode() + SUFFIX).digest()
    reversed_bytes = digest[:8][::-1]
    text = ""
    for index in (0, 3):
        triple = (reversed_bytes[index] << 16) | (reversed_bytes[index + 1] << 8) | reversed_bytes[index + 2]
        text += BASE64[(triple >> 18) & 63] + BASE64[(triple >> 12) & 63] + BASE64[(triple >> 6) & 63] + BASE64[triple & 63]
    tail = (reversed_bytes[6] << 16) | (reversed_bytes[7] << 8)
    return text + BASE64[(tail >> 18) & 63] + BASE64[(tail >> 12) & 63] + BASE64[(tail >> 6) & 63]


def uleb(value):
    out = bytearray()
    while True:
        byte = value & 0x7F
        value >>= 7
        out.append(byte | (0x80 if value else 0))
        if not value:
            return bytes(out)


def read_uleb(data, position):
    value = shift = 0
    while True:
        byte = data[position]
        position += 1
        value |= (byte & 0x7F) << shift
        if not byte & 0x80:
            return value, position
        shift += 7


def build_trie(entries):
    offsets = [0] * len(entries)
    for _ in range(8):
        root = bytearray(b"\0") + bytes([len(entries)])
        for (name, _, _), offset in zip(entries, offsets):
            root += name.encode() + b"\0" + uleb(offset)
        nodes = []
        for name, flags, address in entries:
            terminal = uleb(flags) + uleb(address)
            nodes.append(uleb(len(terminal)) + terminal + b"\0")
        position, expected = len(root), []
        for node in nodes:
            expected.append(position)
            position += len(node)
        if expected == offsets:
            break
        offsets = expected
    trie = bytes(root) + b"".join(nodes)
    return trie + bytes(-len(trie) % 8)


def parse_trie(data, start, size):
    found = {}

    def walk(node, prefix):
        position = start + node
        terminal, position = read_uleb(data, position)
        children = position + terminal
        if terminal:
            flags, position = read_uleb(data, position)
            address, position = read_uleb(data, position)
            found[prefix] = (flags, address)
        count = data[children]
        position = children + 1
        for _ in range(count):
            end = data.index(0, position)
            label = data[position:end].decode()
            child, position = read_uleb(data, end + 1)
            walk(child, prefix + label)

    if size:
        walk(0, "")
    return found


def build_fixups(imports):
    symbols = bytearray(b"\0")
    entries = []
    for name, ordinal in imports:
        entries.append(ordinal | (len(symbols) << 9))
        symbols += name.encode() + b"\0"
    imports_offset = 32
    header = struct.pack("<7I", 0, 28, imports_offset, imports_offset + 4 * len(imports), len(imports), 1, 0)
    blob = header + struct.pack("<I", 0) + b"".join(struct.pack("<I", entry) for entry in entries) + bytes(symbols)
    return blob + bytes(-len(blob) % 8)


def dylib_command(path):
    name = path.encode() + b"\0"
    size = (24 + len(name) + 7) & ~7
    return struct.pack("<IIIIII", LC_LOAD_DYLIB, size, 24, 2, 0x10000, 0x10000) + name.ljust(size - 24, b"\0")


def build_dylib(exports, imports, signed=False, bind_opcodes=False):
    trie = build_trie(exports)
    fixups = build_fixups(imports)
    linkedit = bytearray(trie + fixups)
    trie_command = (struct.pack("<IIII", LC_DYLD_EXPORTS_TRIE, 16, PAGE, len(trie)))
    fixups_command = struct.pack("<IIII", LC_DYLD_CHAINED_FIXUPS, 16, PAGE + len(trie), len(fixups))
    if bind_opcodes:
        trie_command = struct.pack("<IIIIIIIIIIII", LC_DYLD_INFO_ONLY, 48, 0, 0, PAGE, 8, 0, 0, 0, 0, PAGE, len(trie))
    extra = [trie_command, fixups_command, dylib_command("@rpath/libkernel.prx"), dylib_command("/usr/lib/libSystem.B.dylib")]
    if signed:
        extra.append(struct.pack("<IIII", LC_CODE_SIGNATURE, 16, PAGE + len(linkedit), 0))
    text = struct.pack("<II16sQQQQiiII", LC_SEGMENT_64, 72, b"__TEXT", 0, PAGE, 0, PAGE, 5, 5, 0, 0)
    link = struct.pack("<II16sQQQQiiII", LC_SEGMENT_64, 72, b"__LINKEDIT", PAGE, PAGE, PAGE, len(linkedit), 1, 1, 0, 0)
    body = text + link + b"".join(extra)
    header = struct.pack("<IIIIIIII", 0xFEEDFACF, 0x01000007, 3, 6, 2 + len(extra), len(body), 0, 0)
    image = bytearray(header + body)
    image += bytes(PAGE - len(image))
    image += linkedit
    return bytes(image)


def command_fields(data):
    offset, found = 32, {}
    for _ in range(struct.unpack_from("<I", data, 16)[0]):
        cmd, size = struct.unpack_from("<II", data, offset)
        found.setdefault(cmd, []).append(offset)
        offset += size
    return found


def segment(data, name):
    for offset in command_fields(data)[LC_SEGMENT_64]:
        fields = struct.unpack_from("<16sQQQQ", data, offset + 8)
        if fields[0].rstrip(b"\0").decode() == name:
            return offset, fields[1:]
    raise AssertionError(name)


def chained_imports(data):
    offset = command_fields(data)[LC_DYLD_CHAINED_FIXUPS][0]
    base, size = struct.unpack_from("<II", data, offset + 8)
    _, _, imports_offset, symbols_offset, count, _, _ = struct.unpack_from("<7I", data, base)
    result = []
    for index in range(count):
        raw, = struct.unpack_from("<I", data, base + imports_offset + 4 * index)
        start = base + symbols_offset + (raw >> 9)
        result.append((data[start:data.index(0, start)].decode(), raw & 0xFF))
    return result


def patch(patcher, directory, name, image):
    path = Path(directory) / name
    path.write_bytes(image)
    result = subprocess.run([str(patcher), "libtest", str(path)], capture_output=True, text=True, timeout=30)
    return result, path


def exports_of(data):
    offset = command_fields(data)[LC_DYLD_EXPORTS_TRIE][0]
    start, size = struct.unpack_from("<II", data, offset + 8)
    return parse_trie(data, start, size)


def check_shrinking(patcher, directory):
    exports = [("_puts_nid_postfix", 0, 0x100), ("_exit_nid_postfix", 0, 0x108), ("_weak_inline", EXPORT_WEAK, 0x110)]
    imports = [("_malloc", 2), ("_puts_nid_postfix", 1), ("_sceKernelFoo", 1), ("_helper", 1), ("_exit_nid_postfix", 2)]
    result, path = patch(patcher, directory, "shrink.dylib", build_dylib(exports, imports))
    assert result.returncode == 0, (result.stdout, result.stderr)
    data = path.read_bytes()
    assert exports_of(data) == {"_" + nid("puts"): (0, 0x100), "_" + nid("exit"): (0, 0x108), "_weak_inline": (EXPORT_WEAK, 0x110)}, exports_of(data)
    assert chained_imports(data) == [("_malloc", 2), ("_" + nid("puts"), 1), ("_" + nid("sceKernelFoo"), 1), ("_helper", 1), ("_exit_nid_postfix", 2)], chained_imports(data)
    assert len(data) == len(build_dylib(exports, imports)), "the image changed size"


def check_growing(patcher, directory):
    original = build_dylib([("_ab", 0, 0x100), ("_cd", 0, 0x108)], [])
    result, path = patch(patcher, directory, "grow.dylib", original)
    assert result.returncode == 0, (result.stdout, result.stderr)
    data = path.read_bytes()
    assert exports_of(data) == {"_" + nid("ab"): (0, 0x100), "_" + nid("cd"): (0, 0x108)}, exports_of(data)
    offset, (vmaddr, vmsize, fileoff, filesize) = segment(data, "__LINKEDIT")
    assert fileoff + filesize == len(data) and vmsize >= filesize and len(data) > len(original), (fileoff, filesize, len(data))
    trie_offset, trie_size = struct.unpack_from("<II", data, command_fields(data)[LC_DYLD_EXPORTS_TRIE][0] + 8)
    assert trie_offset >= len(original) and trie_offset + trie_size <= len(data) and trie_offset % 8 == 0, (trie_offset, trie_size)


def check_preserved(patcher, directory):
    reference = Path(directory) / "reference.dylib"
    reference.write_bytes(build_dylib([("_puts", 0, 0x100)], []))
    path = Path(directory) / "preserve.dylib"
    path.write_bytes(build_dylib([("_puts_nid_postfix", 0, 0x100), ("_exit_nid_postfix", 0, 0x108)], []))
    result = subprocess.run([str(patcher), "libtest", "--preserve-exports", str(reference), str(path)], capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, (result.stdout, result.stderr)
    assert exports_of(path.read_bytes()) == {"_puts_nid_postfix": (0, 0x100), "_" + nid("exit"): (0, 0x108)}, exports_of(path.read_bytes())


def check_refusals(patcher, directory):
    growing = [("_ab", 0, 0x100), ("_cd", 0, 0x108)]
    result, _ = patch(patcher, directory, "signed.dylib", build_dylib(growing, [], signed=True))
    assert result.returncode != 0 and "signed Mach-O images cannot grow their export trie" in result.stdout + result.stderr, (result.stdout, result.stderr)
    result, _ = patch(patcher, directory, "opcodes.dylib", build_dylib([("_puts_nid_postfix", 0, 0x100)], [], bind_opcodes=True))
    assert result.returncode != 0 and "bind opcodes are not supported" in result.stdout + result.stderr, (result.stdout, result.stderr)
    result, _ = patch(patcher, directory, "long.dylib", build_dylib([("_puts_nid_postfix", 0, 0x100)], [("_sceXy", 1)]))
    assert result.returncode != 0 and "import NID longer than original name" in result.stdout + result.stderr, (result.stdout, result.stderr)


def main():
    patcher = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-macho-nid-") as directory:
        check_shrinking(patcher, directory)
        check_growing(patcher, directory)
        check_preserved(patcher, directory)
        check_refusals(patcher, directory)
    print("NID patcher Mach-O tests passed")


if __name__ == "__main__":
    main()
