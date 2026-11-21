from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent / "macos"))

from qualified_import_fixture import consumer, provider
from guest_import_fixtures import executable
from test_guest_intel_trampolines import main_fixture
from test_macos_executable_exports import exports
from test_macos_output import MachO, uleb


def binds(data):
    macho = MachO(data)
    offset, size = macho.dyld_info[2:4]
    stream = data[offset:offset + size]
    symbols = []
    position = 0
    while position < len(stream):
        opcode = stream[position]
        position += 1
        if opcode == 0x00:
            break
        kind = opcode & 0xF0
        if kind == 0x40:
            end = stream.index(0, position)
            symbols.append(stream[position:end].decode())
            position = end + 1
        elif kind == 0x60:
            while stream[position] & 0x80:
                position += 1
            position += 1
        elif kind == 0x70:
            _, position = uleb(stream, position)
    return symbols


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
    return {name for name in binds(data) if name.endswith("#guest")}


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

        result, output = convert(relinker, work / "consumer", {**twins, "consumer.prx": consumer("b.prx")})
        assert result.returncode == 0, (result.stdout, result.stderr)
        directory = output.parent / "app0/sce_module"
        converted = directory / "consumer.prx.guest.prx"
        imports = guest_imports(converted.read_bytes())
        assert imports == {"_shared#module1#guest"}, imports
        assert set(exports(converted.read_bytes())) == {"_read#guest"}, exports(converted.read_bytes())
        dylibs = MachO(converted.read_bytes()).dylibs
        assert "@loader_path/b.prx.guest.prx" in dylibs and "a.prx.guest.prx" not in " ".join(dylibs), dylibs

        result, output = convert(relinker, work / "ambiguous",
                                 twins, executable("b.prx", symbol="shared", extra_dependencies=("a.prx",)))
        assert result.returncode == 2 and "Ambiguous guest import shared" in result.stderr, (result.returncode, result.stderr)

        result, output = convert(relinker, work / "single-needed",
                                 twins, executable("b.prx", symbol="shared"))
        assert result.returncode == 0, (result.stdout, result.stderr)
        imports = guest_imports(output.read_bytes())
        assert imports == {"_shared#guest"}, imports
        directory = output.parent / "app0/sce_module"
        assert set(exports((directory / "b.prx.guest.prx").read_bytes())) == {"_shared#guest", "_shared#module1#guest"}

        result, output = convert(relinker, work / "single-provider",
                                 {"a.prx": provider(11)}, executable("a.prx"))
        assert result.returncode == 0, (result.stdout, result.stderr)
        imports = guest_imports(output.read_bytes())
        assert imports == {"_shared#guest"}, imports
        assert set(exports((output.parent / "app0/sce_module/a.prx.guest.prx").read_bytes())) == {"_shared#guest"}

        source = executable("b.prx", extra_dependencies=("a.prx",))
        source[0x4898 + 4] = 0x11
        result, output = convert(relinker, work / "type-mismatch", twins, source)
        assert result.returncode == 2 and "Guest import/export type mismatch: shared" in result.stderr, (result.returncode, result.stderr)
        assert not output.exists(), "type-mismatch created an output"
    print("macOS guest qualified import tests passed")


if __name__ == "__main__":
    main()
