import os
from pathlib import Path
import platform
import struct
import subprocess
import sys
import tempfile

from guest_import_fixtures import dynamic, executable
from test_guest_deferred_dependencies import page_aligned
from test_guest_intel_trampolines import PLAIN_SITE, guest_fixture, main_fixture
from test_guest_module_directories import needed_libraries
from test_guest_symbol_names import dynamic_tags, file_offset, imported_symbols, importing_executable, module_symbols, symbol_name


def module(value=None, owner=None, weak=False, data=False, symbol="shared#A#B"):
    image = guest_fixture(PLAIN_SITE)
    names = [symbol]
    if owner is not None:
        names += ["read", owner]
    strings = b"\0"
    offsets = []
    for name in names:
        offsets.append(len(strings))
        strings += name.encode() + b"\0"
    assert len(strings) <= 0x40
    image[0x800:0x800 + len(strings)] = strings
    count = 3 if owner is not None else 2
    struct.pack_into("<II", image, 0x840, 1, count)
    info = (0x20 if weak else 0x10) | (1 if data else 2)
    struct.pack_into("<IBBHQQ", image, 0x898, offsets[0], info, 0,
                     0 if owner is not None else 1,
                     0 if owner is not None else (0x2340 if data else 0x1000),
                     0 if owner is not None else (4 if data else 6))
    tags = [(5, 0x2200), (10, len(strings)), (6, 0x2280), (11, 24),
            (4, 0x2240), (7, 0x2300), (8, 24 if owner is not None else 0), (9, 24)]
    if owner is None:
        if data:
            struct.pack_into("<I", image, 0x940, value)
        else:
            image[0x400:0x406] = b"\xb8" + struct.pack("<I", value) + b"\xc3"
    else:
        if data:
            code = b"\x48\x8b\x05" + struct.pack("<i", 0x2380 - 0x1017) + b"\x8b\x00\xc3"
        else:
            code = b"\xff\x25" + struct.pack("<i", 0x2380 - 0x1016)
        image[0x410:0x410 + len(code)] = code
        struct.pack_into("<IBBHQQ", image, 0x8b0, offsets[1], 0x12, 0, 1, 0x1010, len(code))
        struct.pack_into("<QQq", image, 0x900, 0x2380, (1 << 32) | 6, 0)
        tags += [(1, offsets[2]), (0x61000045, (1 << 48) | offsets[2])]
    dynamic(image, 0x600, 176, tags)
    return page_aligned(image)


def at_zero(image):
    struct.pack_into("<Q", image, 0x2000 + 0x298 + 8, 0)
    struct.pack_into("<Q", image, 24, 0)
    struct.pack_into("<QQ", image, 64 + 16, 0, 0)
    return image


def tls_module(value=None, owner=None):
    image = module(value, owner, data=True)
    struct.pack_into("<B", image, 0x2298 + 4, 0x16)
    if owner is None:
        struct.pack_into("<QQ", image, 0x2298 + 8, 0, 4)
        struct.pack_into("<H", image, 56, 4)
        struct.pack_into("<IIQQQQQQ", image, 232,
                         7, 4, 0x23e0, 0x23e0, 0x23e0, 4, 4, 4)
        struct.pack_into("<I", image, 0x23e0, value)
        return image
    strings = b"\0shared#A#B\0read\0" + owner.encode() + b"\0vNe1w4diLCs\0"
    image[0x2200:0x2200 + len(strings)] = strings
    resolver = strings.index(b"vNe1w4diLCs")
    struct.pack_into("<II", image, 0x2240, 1, 4)
    struct.pack_into("<IBBHQQ", image, 0x22c8, resolver, 0x12, 0, 0, 0, 0)
    code = b"\x48\x8d\x3d" + struct.pack("<i", 0x2380 - 0x1017)
    code += b"\x48\x83\xec\x08\xff\x15" + struct.pack("<i", 0x2390 - 0x1021)
    code += b"\x48\x83\xc4\x08\x8b\x00\xc3"
    image[0x1010:0x1010 + len(code)] = code
    struct.pack_into("<Q", image, 0x22b0 + 16, len(code))
    for index, (kind, symbol) in enumerate(((16, 1), (17, 1), (6, 3))):
        struct.pack_into("<QQq", image, 0x2300 + index * 24, 0x2380 + index * 8, (symbol << 32) | kind, 0)
    dynamic(image, 0x2000, 176, [(5, 0x2200), (10, len(strings)), (6, 0x2280), (11, 24),
                                (4, 0x2240), (7, 0x2300), (8, 72), (9, 24),
                                (1, strings.index(owner.encode())),
                                (0x61000045, (1 << 48) | strings.index(owner.encode()))])
    return image


def host_module():
    image = module(33, symbol="shared")
    strings = b"\0shared\0host.prx\0"
    image[0x2200:0x2200 + len(strings)] = strings
    struct.pack_into("<IIIII", image, 0x2240, 1, 2, 1, 0, 0)
    dynamic(image, 0x2000, 176, [(5, 0x2200), (10, len(strings)), (6, 0x2280),
                                (11, 24), (4, 0x2240), (14, 8)])
    struct.pack_into("<H", image, 56, 4)
    struct.pack_into("<IIQQQQQQ", image, 232, 0x6474e551, 6, 0, 0, 0, 0, 0, 16)
    return image


def call_and_exit(image, slots):
    struct.pack_into("<I", image, 68, 7)
    code = bytearray(b"\x53")
    for index, slot in enumerate(slots):
        code += b"\xff\x15" + struct.pack("<i", slot - (0x4000 + len(code) + 6))
        code += b"\x89\xc3" if index == 0 else b"\x6b\xdb\x0a\x01\xc3"
    code += b"\x89\xdf\xb8\x3c\0\0\0\x0f\x05"
    image[0x4000:0x4000 + len(code)] = code
    return image


def relocation_bindings(image):
    tags = dynamic_tags(image)
    bindings = []
    for address, size in ((7, 8), (23, 2)):
        if address not in tags or tags.get(size, 0) == 0:
            continue
        table = file_offset(image, tags[address])
        for position in range(table, table + tags[size], 24):
            target, info = struct.unpack_from("<QQ", image, position)
            if info >> 32:
                bindings.append((target, symbol_name(image, tags, info >> 32), info & 0xffffffff))
    return sorted(bindings)


def defined_symbols(image):
    tags = dynamic_tags(image)
    table = file_offset(image, tags[6])
    count, = struct.unpack_from("<I", image, file_offset(image, tags[4]) + 4)
    symbols = {}
    for index in range(1, count):
        fields = struct.unpack_from("<BBHQQ", image, table + index * 24 + 4)
        if fields[2] != 0:
            name = symbol_name(image, tags, index)
            assert name not in symbols, name
            symbols[name] = fields
    return symbols


def convert(relinker, case, modules, source=None, options=()):
    directory = case / "sce_module"
    directory.mkdir(parents=True)
    for name, image in modules.items():
        (directory / name).write_bytes(image)
    input_path = case / "input.elf"
    input_path.write_bytes(main_fixture() if source is None else source)
    output = case / "output.elf"
    result = subprocess.run([str(relinker), *options, str(input_path), str(output)],
                            capture_output=True, text=True, timeout=20)
    return result, output


def probe(directory, data, alias):
    code = (
        "import ctypes, os, sys\n"
        "from pathlib import Path\n"
        "root = Path(sys.argv[1])\n"
        "first = ctypes.CDLL(str(root / 'a.prx.guest.prx'), mode=os.RTLD_NOW | os.RTLD_GLOBAL)\n"
        "second = ctypes.CDLL(str(root / 'b.prx.guest.prx'), mode=os.RTLD_NOW | os.RTLD_GLOBAL)\n"
        "consumer = ctypes.CDLL(str(root / 'consumer.prx.guest.prx'), mode=os.RTLD_NOW)\n"
        "read = getattr(consumer, 'read#guest')\n"
        "read.restype = ctypes.c_int\n"
        "value = read()\n"
        "assert value == 22, ('qualified import selected wrong provider', value)\n"
        "if sys.argv[2] == 'data':\n"
        "    assert ctypes.c_int.in_dll(first, 'shared#guest').value == 11\n"
        "    assert ctypes.c_int.in_dll(second, 'shared#guest').value == 22\n"
        "    assert ctypes.addressof(ctypes.c_int.in_dll(second, sys.argv[3])) == ctypes.addressof(ctypes.c_int.in_dll(second, 'shared#guest'))\n"
        "else:\n"
        "    assert getattr(first, 'shared#guest')() == 11\n"
        "    assert getattr(second, 'shared#guest')() == 22\n"
        "    assert ctypes.cast(getattr(second, sys.argv[3]), ctypes.c_void_p).value == ctypes.cast(getattr(second, 'shared#guest'), ctypes.c_void_p).value\n"
    )
    result = subprocess.run([sys.executable, "-c", code, str(directory), "data" if data else "function", alias],
                            capture_output=True, text=True, timeout=20)
    assert result.returncode == 0, (result.returncode, result.stdout, result.stderr)


def main():
    relinker = Path(sys.argv[1]).resolve()
    runnable = sys.platform.startswith("linux") and platform.machine() in ("x86_64", "AMD64")
    with tempfile.TemporaryDirectory(prefix="anyps5-linux-qualified-imports-") as temporary:
        work = Path(temporary)
        for label, weak, weak_definition, data, zero, tls, protected in (
                ("function", False, False, False, False, False, False),
                ("weak-import", True, False, False, False, False, False),
                ("weak-definition", False, True, False, False, False, False),
                ("data", False, False, True, False, False, False),
                ("zero-address", False, False, False, True, False, False),
                ("tls", False, False, True, False, True, False),
                ("protected-definition", False, False, False, False, False, True)):
            first = tls_module(11) if tls else module(11, data=data)
            second = tls_module(22) if tls else module(22, weak=weak_definition, data=data)
            consumer = tls_module(owner="b.prx") if tls else module(owner="b.prx", weak=weak, data=data)
            if zero:
                second = at_zero(second)
            if protected:
                second[0x2298 + 5] = 3
            result, output = convert(relinker, work / label, {
                "a.prx": first,
                "b.prx": second,
                "consumer.prx": consumer,
            })
            assert result.returncode == 0, (label, result.stdout, result.stderr)
            directory = output.parent / "app0/sce_module"
            if label == "function":
                artifacts = {path: path.read_bytes() for path in [output, *sorted(directory.glob("*.guest.prx"))]}
                result = subprocess.run([str(relinker), str(output.parent / "input.elf"), str(output)],
                                        capture_output=True, text=True, timeout=20)
                assert result.returncode == 0, (result.stdout, result.stderr)
                assert set(artifacts) == {output, *directory.glob("*.guest.prx")}
                for path, image in artifacts.items():
                    assert path.read_bytes() == image, ("Repeated conversion changed output", path.name)
            for name in ("a", "b"):
                symbols = defined_symbols((directory / f"{name}.prx.guest.prx").read_bytes())
                assert "shared#guest" in symbols, symbols
                aliases = set(symbols) - {"shared#guest"}
                assert len(aliases) == 1, symbols
                alias = aliases.pop()
                assert symbols[alias] == symbols["shared#guest"], (label, name, symbols)
            consumer = (directory / "consumer.prx.guest.prx").read_bytes()
            expected = ["$ORIGIN/b.prx.guest.prx"] + (["ld-linux-x86-64.so.2"] if tls else [])
            assert needed_libraries(consumer) == expected, needed_libraries(consumer)
            imports = imported_symbols(consumer) - {"__tls_get_addr"}
            assert len(imports) == 1, imports
            alias = next(iter(imports))
            if runnable:
                probe(directory, data, alias)
            assert alias != "shared#guest" and alias.endswith("#guest"), alias
            assert alias in module_symbols((directory / "b.prx.guest.prx").read_bytes())
            assert alias not in module_symbols((directory / "a.prx.guest.prx").read_bytes())

        twins = {"a.prx": module(11), "b.prx": module(22)}
        result, output = convert(relinker, work / "executable", twins,
                                 call_and_exit(executable("b.prx", extra_dependencies=("a.prx",)), [0x4a20]),
                                 ["--skip-syscall-check"])
        assert result.returncode == 0, (result.stdout, result.stderr)
        imports = imported_symbols(output.read_bytes())
        assert len(imports) == 1, imports
        directory = output.parent / "app0/sce_module"
        assert imports <= set(module_symbols((directory / "b.prx.guest.prx").read_bytes()))
        assert imports.isdisjoint(module_symbols((directory / "a.prx.guest.prx").read_bytes()))
        if runnable:
            output.chmod(0o755)
            run = subprocess.run([str(output)], capture_output=True, text=True, timeout=20)
            assert run.returncode == 22, (run.returncode, run.stdout, run.stderr)

        result, output = convert(relinker, work / "executable-two-providers", twins,
                                 call_and_exit(importing_executable(["shared#A#B", "shared#A#C"], ["a", "b"]), [0x4e00, 0x4e08]),
                                 ["--skip-syscall-check"])
        assert result.returncode == 0, (result.stdout, result.stderr)
        imports = imported_symbols(output.read_bytes())
        assert len(imports) == 2, imports
        directory = output.parent / "app0/sce_module"
        first = set(module_symbols((directory / "a.prx.guest.prx").read_bytes()))
        second = set(module_symbols((directory / "b.prx.guest.prx").read_bytes()))
        assert len(imports & first) == len(imports & second) == 1, (imports, first, second)
        assert not imports & first & second, (imports, first, second)
        bindings = relocation_bindings(output.read_bytes())
        assert len(bindings) == 2 and bindings[0][1] in first and bindings[1][1] in second, bindings
        if runnable:
            output.chmod(0o755)
            run = subprocess.run([str(output)], capture_output=True, text=True, timeout=20)
            assert run.returncode == 132, (run.returncode, run.stdout, run.stderr)

        for duplicate in (False, True):
            for data in (False, True):
                second = module(22, data=data)
                second[0x2298 + 4] = 0x10
                modules = {"b.prx": second}
                if duplicate:
                    modules["a.prx"] = module(11, data=data)
                source = executable("b.prx", extra_dependencies=("a.prx",) if duplicate else ())
                source[0x4898 + 4] = 0x11 if data else 0x12
                if data:
                    code = b"\x48\x8b\x05" + struct.pack("<i", 0x4a20 - 0x4007)
                    code += b"\x8b\x38\xb8\x3c\0\0\0\x0f\x05"
                    source[0x4000:0x4000 + len(code)] = code
                else:
                    call_and_exit(source, [0x4a20])
                result, output = convert(relinker, work / f"untyped-{duplicate}-{data}", modules, source,
                                         ["--skip-syscall-check"])
                assert result.returncode == 0, (duplicate, data, result.stdout, result.stderr)
                if runnable:
                    output.chmod(0o755)
                    run = subprocess.run([str(output)], capture_output=True, text=True, timeout=20)
                    assert run.returncode == 22, (duplicate, data, run.returncode, run.stdout, run.stderr)

        self_import = module(owner="b.prx")
        struct.pack_into("<II", self_import, 0x2240, 1, 4)
        struct.pack_into("<IBBHQQ", self_import, 0x22c8, 1, 0x12, 0, 1, 0x1000, 6)
        self_import[0x1000:0x1006] = b"\xb8\x16\0\0\0\xc3"
        result, output = convert(relinker, work / "self", {"a.prx": module(11), "b.prx": self_import})
        assert result.returncode == 0, (result.stdout, result.stderr)
        if runnable:
            directory = output.parent / "app0/sce_module"
            code = ("import ctypes, os, sys; from pathlib import Path; p=Path(sys.argv[1]); "
                    "a=ctypes.CDLL(str(p/'a.prx.guest.prx'), mode=os.RTLD_GLOBAL); "
                    "b=ctypes.CDLL(str(p/'b.prx.guest.prx')); assert getattr(b, 'read#guest')() == 22")
            run = subprocess.run([sys.executable, "-c", code, str(directory)], capture_output=True, text=True, timeout=20)
            assert run.returncode == 0, (run.returncode, run.stdout, run.stderr)

        case = work / "multiple-executables"
        case.mkdir()
        secondary = case / "second.elf"
        secondary.write_bytes(call_and_exit(executable("b.prx"), [0x4a20]))
        result, output = convert(relinker, case, twins, options=["--skip-syscall-check", "--executable", secondary.name])
        assert result.returncode == 0, (result.stdout, result.stderr)
        secondary_output = output.parent / "second.elf.elf"
        bindings = relocation_bindings(secondary_output.read_bytes())
        assert len(bindings) == 1 and bindings[0][0] == 0x404a20 and bindings[0][2] == 6, bindings
        directory = output.parent / "app0/sce_module"
        assert bindings[0][1] in defined_symbols((directory / "b.prx.guest.prx").read_bytes()), bindings
        assert bindings[0][1] not in defined_symbols((directory / "a.prx.guest.prx").read_bytes()), bindings
        if runnable:
            secondary_output.chmod(0o755)
            run = subprocess.run([str(secondary_output)], capture_output=True, text=True, timeout=20)
            assert run.returncode == 22, (run.returncode, run.stdout, run.stderr)

        for label, second, info, error in (
                ("type-mismatch", module(22, data=True), 0x12, "Guest import/export type mismatch: shared"),
                ("non-tls", tls_module(22), 0x10, "Non-TLS relocation against guest TLS export: shared")):
            source = executable("b.prx", extra_dependencies=("a.prx",))
            source[0x4898 + 4] = info
            result, _ = convert(relinker, work / label, {"a.prx": module(11), "b.prx": second}, source)
            assert result.returncode == 2 and error in result.stderr, (result.returncode, result.stdout, result.stderr)

        second = module(22)
        second[0x2298 + 4] = 0x10
        source = executable("b.prx", extra_dependencies=("a.prx",))
        source[0x4898 + 4] = 0x16
        struct.pack_into("<Q", source, 0x4900 + 8, (1 << 32) | 16)
        result, _ = convert(relinker, work / "tls-untyped-export", {"a.prx": module(11), "b.prx": second}, source)
        assert result.returncode == 2 and "Guest import/export type mismatch: shared" in result.stderr, result.stderr

        result, _ = convert(relinker, work / "ambiguous", {
            **twins, "consumer.prx": module(owner="b.prx", symbol="shared"),
        })
        assert result.returncode == 2 and "Ambiguous guest import shared" in result.stderr, result.stderr

        for owner, modules in (("host.prx", twins), ("b.prx", {"a.prx": module(11), "b.prx": module(22, symbol="other")})):
            result, output = convert(relinker, work / ("fallback-" + owner), {
                **modules, "consumer.prx": module(owner=owner),
            })
            assert result.returncode == 0, (owner, result.stdout, result.stderr)
            directory = output.parent / "app0/sce_module"
            consumer = (directory / "consumer.prx.guest.prx").read_bytes()
            assert "shared" in imported_symbols(consumer), imported_symbols(consumer)
            assert "shared#guest" not in imported_symbols(consumer), imported_symbols(consumer)
            assert relocation_bindings(consumer) == [(0x2380, "shared", 6)], relocation_bindings(consumer)
            if runnable:
                host = output.parent / "host.prx"
                host.write_bytes(host_module())
                code = (
                    "import ctypes, os, sys\n"
                    "from pathlib import Path\n"
                    "host = ctypes.CDLL(sys.argv[1], mode=os.RTLD_NOW | os.RTLD_GLOBAL)\n"
                    "root = Path(sys.argv[2])\n"
                    "a = ctypes.CDLL(str(root / 'a.prx.guest.prx'), mode=os.RTLD_NOW | os.RTLD_GLOBAL)\n"
                    "b = ctypes.CDLL(str(root / 'b.prx.guest.prx'), mode=os.RTLD_NOW | os.RTLD_GLOBAL)\n"
                    "consumer = ctypes.CDLL(str(root / 'consumer.prx.guest.prx'), mode=os.RTLD_NOW)\n"
                    "read = getattr(consumer, 'read#guest')\n"
                    "read.restype = ctypes.c_int\n"
                    "assert read() == 33\n"
                )
                run = subprocess.run([sys.executable, "-c", code, str(host), str(directory)],
                                     capture_output=True, text=True, timeout=20)
                assert run.returncode == 0, (owner, run.returncode, run.stdout, run.stderr)
    print("Linux qualified guest import tests passed")


if __name__ == "__main__":
    main()
