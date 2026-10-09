import base64
import hashlib
import struct
import subprocess
import sys
import tempfile
from pathlib import Path


RAW = 0x200
RVA = 0x1000
NAMES = ("a", "b", "sceOrdinaryExport")
ORDINALS = (2, 0, 1)


def nid(name):
    suffix = bytes.fromhex("518d64a635ded8c1e6b039b1c3e55230")
    digest = hashlib.sha1(name.encode() + suffix).digest()[:8][::-1]
    return base64.b64encode(digest, altchars=b"+-").decode().rstrip("=")


def make_image(is64, layout):
    image = bytearray(0x400)
    image[:2] = b"MZ"
    struct.pack_into("<I", image, 0x3C, 0x80)
    image[0x80:0x84] = b"PE\0\0"
    optional_size = 240 if is64 else 224
    struct.pack_into("<HHIIIHH", image, 0x84, 0x8664 if is64 else 0x14C,
                     1, 0, 0, 0, optional_size, 0x2022)
    optional = 0x98
    struct.pack_into("<H", image, optional, 0x20B if is64 else 0x10B)
    struct.pack_into("<II", image, optional + 32, 0x1000, 0x200)
    directory = optional + (112 if is64 else 96)
    section = optional + optional_size
    struct.pack_into("<8sIIIIIIHHI", image, section, b".edata\0\0", 0x200,
                     RVA, 0x200, RAW, 0, 0, 0, 0, 0x40000040)
    struct.pack_into("<IIHHIIIIIII", image, RAW, 0, 0, 0, 0, RVA + 0x70,
                     1, 5, len(NAMES), RVA + 0x40, RVA + 0x54, RVA + 0x60)
    image[RAW + 0x70:RAW + 0x7C] = b"fixture.dll\0"
    cursor = 0x90
    forwarders = {}

    def put(text):
        nonlocal cursor
        address = RVA + cursor
        data = text.encode() + b"\0"
        image[RAW + cursor:RAW + cursor + len(data)] = data
        cursor += len(data)
        return address

    def forward(text):
        address = put(text)
        forwarders[address] = text
        return address

    before = forward("KERNEL32.GetCurrentThreadId") if layout == "before" else None
    names = [put(NAMES[0])]
    first = forward("KERNEL32.GetCurrentProcessId")
    names.append(put(NAMES[1]))
    second = before or forward("KERNEL32.GetCurrentThreadId")
    names.append(put(NAMES[2]))
    ordinal_only = forward("OTHER.#123")
    functions = [second, 0x2000, first, ordinal_only, first]
    if layout == "ordinary":
        functions = [0x2000 + i * 16 for i in range(5)]
        forwarders = {}
    struct.pack_into("<5I", image, RAW + 0x40, *functions)
    struct.pack_into("<3I", image, RAW + 0x54, *names)
    struct.pack_into("<3H", image, RAW + 0x60, *ORDINALS)
    struct.pack_into("<II", image, directory, RVA, cursor)
    return image, functions, forwarders, directory


def read_string(image, rva):
    offset = RAW + rva - RVA
    return image[offset:image.index(0, offset)].decode()


def run_patcher(patcher, path):
    return subprocess.run([patcher, "libfixture", str(path)], capture_output=True,
                          text=True, timeout=10)


def check_valid(patcher, directory, is64, layout):
    image, functions, forwarders, data_directory = make_image(is64, layout)
    path = directory / f"{is64}-{layout}.dll"
    path.write_bytes(image)
    result = run_patcher(patcher, path)
    if result.returncode != 0:
        raise AssertionError(result.stdout + result.stderr)
    patched = path.read_bytes()
    assert list(struct.unpack_from("<5I", patched, RAW + 0x40)) == functions
    assert read_string(patched, RVA + 0x70) == "fixture.dll"
    for address, text in forwarders.items():
        actual = read_string(patched, address)
        assert actual == text, f"forwarder corrupted: {text!r} became {actual!r}"
    name_addresses = struct.unpack_from("<3I", patched, RAW + 0x54)
    names = [read_string(patched, address) for address in name_addresses]
    export_rva, export_size = struct.unpack_from("<II", patched, data_directory)
    assert max(address + len(name) + 1 for address, name in zip(name_addresses, names)) <= export_rva + export_size
    ordinals = struct.unpack_from("<3H", patched, RAW + 0x60)
    assert names == sorted(nid(name) for name in NAMES)
    assert dict(zip(names, ordinals)) == dict(zip(map(nid, NAMES), ORDINALS))


def check_rejected(patcher, directory, is64, mode):
    image, _, forwarders, data_directory = make_image(is64, "interleaved")
    first = min(forwarders)
    if mode == "capacity":
        start = RAW + first - RVA
        image[start:] = b"X" * (len(image) - start - 1) + b"\0"
        struct.pack_into("<5I", image, RAW + 0x40, first, 0x2000, first, first, first)
        struct.pack_into("<3I", image, RAW + 0x54, RVA + 0x90, RVA + 0x90, RVA + 0x90)
        struct.pack_into("<I", image, RAW + 24, 1)
        struct.pack_into("<I", image, data_directory + 4, 0x200)
        expected = "not enough raw space"
    elif mode == "directory-boundary":
        struct.pack_into("<I", image, data_directory + 4, first - RVA + 3)
        expected = "export forwarder string out of bounds"
    else:
        start = RAW + first - RVA
        image[start:] = b"X" * (len(image) - start)
        struct.pack_into("<I", image, RAW + 24, 1)
        struct.pack_into("<I", image, data_directory + 4, 0x200)
        expected = "export forwarder string out of bounds"
    path = directory / f"{is64}-{mode}.dll"
    path.write_bytes(image)
    result = run_patcher(patcher, path)
    assert result.returncode == 2, result.stdout + result.stderr
    assert expected in result.stderr, result.stderr
    assert path.read_bytes() == image, "failed patch modified the input file"


def main():
    patcher = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory() as temporary:
        directory = Path(temporary)
        for is64 in (False, True):
            for layout in ("interleaved", "before", "ordinary"):
                check_valid(patcher, directory, is64, layout)
            for mode in ("capacity", "directory-boundary", "unterminated"):
                check_rejected(patcher, directory, is64, mode)
    print("PE32 and PE32+ forwarded exports preserved; invalid layouts rejected")


if __name__ == "__main__":
    main()
