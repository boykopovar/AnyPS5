from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent / "macos"))

from qualified_import_fixture import consumer, provider, qualified_executable
from guest_import_fixtures import executable
from test_guest_intel_trampolines import main_fixture
from test_linux_guest_qualified_imports import tls_module
from test_macos_executable_exports import exports
from test_macos_output import MachO, uleb

MH_TWOLEVEL = 0x80
MH_FLAT_NAMESPACE = 0x100
SET_DYLIB_ORDINAL_IMM = 0x10
SET_DYLIB_ORDINAL_ULEB = 0x20
SET_DYLIB_SPECIAL_IMM = 0x30
FLAT_LOOKUP = -2


def binds(data):
    macho = MachO(data)
    offset, size = macho.dyld_info[2:4]
    stream = data[offset:offset + size]
    events = []
    position = 0
    ordinal = None
    opcode = None
    while position < len(stream):
        raw = stream[position]
        position += 1
        if raw == 0x00:
            break
        kind = raw & 0xF0
        if kind == SET_DYLIB_ORDINAL_IMM:
            ordinal = raw & 0x0F
            opcode = raw
        elif kind == SET_DYLIB_ORDINAL_ULEB:
            ordinal, position = uleb(stream, position)
            opcode = raw
        elif kind == SET_DYLIB_SPECIAL_IMM:
            ordinal = (raw & 0x0F) - 16 if raw & 0x08 else raw & 0x0F
            opcode = raw
        elif kind == 0x40:
            end = stream.index(0, position)
            events.append({"symbol": stream[position:end].decode(), "dylib_opcode": opcode, "dylib": ordinal})
            position = end + 1
        elif kind == 0x60:
            while stream[position] & 0x80:
                position += 1
            position += 1
        elif kind == 0x70:
            _, position = uleb(stream, position)
        elif kind == 0x50:
            continue
        elif kind == 0x90:
            continue
        else:
            raise AssertionError(f"unsupported bind opcode {raw:#x}")
    return macho, events


def ordinal_of(macho, dylib):
    return macho.dylibs.index(dylib) + 1


def convert(relinker, case, modules, source=None):
    directory = case / "sce_module"
    directory.mkdir(parents=True)
    for name, image in modules.items():
        (directory / name).write_bytes(image)
    input_path = case / "input.elf"
    input_path.write_bytes(main_fixture() if source is None else source)
    output = case / "output"
    result = subprocess.run([str(relinker), "--macos", str(input_path), str(output)],
                            capture_output=True, text=True, timeout=30)
    return result, output


def guest_imports(data):
    _, events = binds(data)
    return {event["symbol"] for event in events if event["symbol"].endswith("#guest")}


def check_two_level(name, data):
    macho = MachO(data)
    assert macho.flags & MH_TWOLEVEL, (name, hex(macho.flags))
    assert not macho.flags & MH_FLAT_NAMESPACE, (name, hex(macho.flags))
    assert macho.dylibs[0] == "/usr/lib/libSystem.B.dylib", macho.dylibs
    return macho


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-macos-qualified-imports-") as directory:
        work = Path(directory)
        twins = {"a.prx": provider(11), "b.prx": provider(22)}

        result, output = convert(relinker, work / "declared-executable",
                                 twins, executable("b.prx", extra_dependencies=("a.prx",)))
        assert result.returncode == 0, (result.stdout, result.stderr)
        imports = guest_imports(output.read_bytes())
        assert imports == {"_shared#module1#guest"}, imports
        directory = output.parent / "app0/sce_module"
        first = exports((directory / "a.prx.guest.prx").read_bytes())
        second = exports((directory / "b.prx.guest.prx").read_bytes())
        assert set(first) == {"_shared#guest", "_shared#module0#guest"}, first
        assert set(second) == {"_shared#guest", "_shared#module1#guest"}, second
        assert first["_shared#module0#guest"] == first["_shared#guest"], first
        assert second["_shared#module1#guest"] == second["_shared#guest"], second

        image = output.read_bytes()
        macho = check_two_level("declared-executable", image)
        _, events = binds(image)
        declared = {event["symbol"]: event for event in events}
        provider_ordinal = ordinal_of(macho, "@executable_path/app0/sce_module/b.prx.guest.prx")
        assert declared["_shared#module1#guest"]["dylib"] == provider_ordinal, declared
        assert declared["_shared#module1#guest"]["dylib_opcode"] == SET_DYLIB_ORDINAL_IMM | provider_ordinal, declared
        assert declared["_Aps5StartGuest_nid_no_patch"]["dylib"] == FLAT_LOOKUP, declared

        result, output = convert(relinker, work / "consumer", {**twins, "consumer.prx": consumer("b.prx")})
        assert result.returncode == 0, (result.stdout, result.stderr)
        directory = output.parent / "app0/sce_module"
        converted = directory / "consumer.prx.guest.prx"
        imports = guest_imports(converted.read_bytes())
        assert imports == {"_shared#module1#guest"}, imports
        assert set(exports(converted.read_bytes())) == {"_read#guest"}, exports(converted.read_bytes())
        image = converted.read_bytes()
        macho = check_two_level("consumer", image)
        dylibs = macho.dylibs
        assert "@loader_path/b.prx.guest.prx" in dylibs and "a.prx.guest.prx" not in " ".join(dylibs), dylibs
        _, events = binds(image)
        shared = next(event for event in events if event["symbol"] == "_shared#module1#guest")
        module_ordinal = ordinal_of(macho, "@loader_path/b.prx.guest.prx")
        assert shared["dylib"] == module_ordinal, (shared, dylibs)
        assert shared["dylib_opcode"] == SET_DYLIB_ORDINAL_IMM | module_ordinal, (shared, dylibs)

        result, output = convert(relinker, work / "tls-consumer",
                                 {"a.prx": tls_module(11), "b.prx": tls_module(22), "consumer.prx": tls_module(owner="b.prx")})
        assert result.returncode == 0, (result.stdout, result.stderr)
        image = (output.parent / "app0/sce_module/consumer.prx.guest.prx").read_bytes()
        macho = check_two_level("tls-consumer", image)
        tls = next(event for event in binds(image)[1] if event["symbol"] == "_shared#module1#guest")
        tls_ordinal = ordinal_of(macho, "@loader_path/b.prx.guest.prx")
        assert tls["dylib"] == tls_ordinal, (tls, macho.dylibs)
        assert tls["dylib_opcode"] == SET_DYLIB_ORDINAL_IMM | tls_ordinal, (tls, macho.dylibs)

        result, output = convert(relinker, work / "two-providers", twins,
                                 qualified_executable(["shared#A#B", "shared#A#C"], ["a.prx", "b.prx"]))
        assert result.returncode == 0, (result.stdout, result.stderr)
        image = output.read_bytes()
        macho = check_two_level("two-providers", image)
        _, events = binds(image)
        bindings = {event["symbol"]: event for event in events}
        first_ordinal = ordinal_of(macho, "@executable_path/app0/sce_module/a.prx.guest.prx")
        second_ordinal = ordinal_of(macho, "@executable_path/app0/sce_module/b.prx.guest.prx")
        assert first_ordinal == 2 and second_ordinal == 3, macho.dylibs
        assert bindings["_shared#module0#guest"]["dylib"] == first_ordinal, bindings
        assert bindings["_shared#module0#guest"]["dylib_opcode"] == SET_DYLIB_ORDINAL_IMM | first_ordinal, bindings
        assert bindings["_shared#module1#guest"]["dylib"] == second_ordinal, bindings
        assert bindings["_shared#module1#guest"]["dylib_opcode"] == SET_DYLIB_ORDINAL_IMM | second_ordinal, bindings
        assert bindings["_Aps5StartGuest_nid_no_patch"]["dylib"] == FLAT_LOOKUP, bindings
        assert len(bindings) == 3, bindings

        result, output = convert(relinker, work / "ambiguous",
                                 twins, executable("b.prx", symbol="shared", extra_dependencies=("a.prx",)))
        assert result.returncode == 2 and "Ambiguous guest import shared" in result.stderr, (result.returncode, result.stderr)

        result, output = convert(relinker, work / "single-needed",
                                 twins, executable("b.prx", symbol="shared"))
        assert result.returncode == 0, (result.stdout, result.stderr)
        imports = guest_imports(output.read_bytes())
        assert imports == {"_shared#guest"}, imports
        image = output.read_bytes()
        bound = {event["symbol"]: event for event in binds(image)[1]}["_shared#guest"]
        assert bound["dylib"] == FLAT_LOOKUP and bound["dylib_opcode"] == SET_DYLIB_SPECIAL_IMM | 0x0E, bound
        directory = output.parent / "app0/sce_module"
        assert set(exports((directory / "b.prx.guest.prx").read_bytes())) == {"_shared#guest", "_shared#module1#guest"}

        result, output = convert(relinker, work / "single-provider",
                                 {"a.prx": provider(11)}, executable("a.prx"))
        assert result.returncode == 0, (result.stdout, result.stderr)
        imports = guest_imports(output.read_bytes())
        assert imports == {"_shared#guest"}, imports
        image = output.read_bytes()
        macho = check_two_level("single-provider", image)
        bound = {event["symbol"]: event for event in binds(image)[1]}["_shared#guest"]
        assert bound["dylib"] == ordinal_of(macho, "@executable_path/app0/sce_module/a.prx.guest.prx"), bound
        assert set(exports((output.parent / "app0/sce_module/a.prx.guest.prx").read_bytes())) == {"_shared#guest"}

        source = executable("b.prx", extra_dependencies=("a.prx",))
        source[0x4898 + 4] = 0x11
        result, output = convert(relinker, work / "type-mismatch", twins, source)
        assert result.returncode == 2 and "Guest import/export type mismatch: shared" in result.stderr, (result.returncode, result.stderr)
        assert not output.exists(), "type-mismatch created an output"
    print("macOS guest qualified import tests passed")


if __name__ == "__main__":
    main()
