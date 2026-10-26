import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import PLAIN_SITE, guest_fixture
from test_guest_symbol_names import DYNAMIC, RELOCATIONS, STRINGS, SYMBOLS, importing_executable

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))
import import_audit


def named_module(tag, name="Party.prx"):
    image = guest_fixture(PLAIN_SITE)
    strings = b"\0" + name.encode() + b"\0"
    image[0x800:0x800 + len(strings)] = strings
    struct.pack_into("<Q", image, 0x618, len(strings))
    struct.pack_into("<qQqQ", image, 0x680, tag, 1, 0, 0)
    struct.pack_into("<QQ", image, 208, 10 * 16, 10 * 16)
    return image


def executable_with_dependencies(libraries):
    image = importing_executable([], modules=libraries)
    dynamic_size, = struct.unpack_from("<Q", image, 152)
    struct.pack_into("<qQqQ", image, 0x4800 + dynamic_size - 16, 0x6100003F, 24, 0, 0)
    struct.pack_into("<QQ", image, 152, dynamic_size + 16, dynamic_size + 16)
    return image


def executable_needing(name):
    image = importing_executable([])
    strings = b"\0" + name.encode() + b"\0"
    image[STRINGS:STRINGS + len(strings)] = strings
    tags = [(1, 1), (5, STRINGS), (10, len(strings)), (6, SYMBOLS), (11, 24),
            (7, RELOCATIONS), (8, 0), (9, 24), (0, 0)]
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, DYNAMIC + index * 16, *tag)
    struct.pack_into("<QQ", image, 152, len(tags) * 16, len(tags) * 16)
    return image


def dependency_names(relinker, work):
    names = ("Party.prx", "", "$LIB.prx", "$ORIGIN/x.prx", "a:b.prx", "a\\b.prx", "a\nb.prx", "a\rb.prx")
    for index, name in enumerate(names):
        source = work / f"dependency-{index}.elf"
        source.write_bytes(executable_needing(name))
        for target, flags in enumerate(([], ["--lazy-binding"], ["--windows"], ["--macos"])):
            output = work / f"dependency-{index}-{target}.out"
            result = subprocess.run([str(relinker), "--skip-sce-module", *flags, str(source), str(output)],
                                    capture_output=True, timeout=20)
            stderr = result.stderr.replace(b"\r\n", b"\n")
            if index == 0:
                assert result.returncode == 0 and output.is_file(), (flags, result.stdout, stderr)
                continue
            expected = f"FAIL: Invalid dependency: {name} (offset {STRINGS + 1:#x})\n".encode()
            assert result.returncode == 2 and stderr.startswith(expected) and not output.exists(), (
                name, flags, result.returncode, stderr)


def slash_dependency(relinker, work):
    for target, flags in enumerate(([], ["--windows"], ["--macos"])):
        case = work / f"slash-{target}"
        modules = case / "sce_module"
        modules.mkdir(parents=True)
        (modules / "engine.prx").write_bytes(named_module(14, "dir/Party.prx"))
        source = case / "input.elf"
        source.write_bytes(executable_needing("dir/Party.prx"))
        output = case / "output.out"
        result = subprocess.run([str(relinker), *flags, str(source), str(output)],
                                capture_output=True, timeout=20)
        assert result.returncode == 0 and output.is_file(), (flags, result.stdout, result.stderr)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-needed-libraries-") as directory:
        work = Path(directory)
        dependency_names(relinker, work)
        slash_dependency(relinker, work)
        for windows in (False, True):
            for bundled in (False, True):
                case = work / f"{windows}-{bundled}"
                modules = case / "sce_module"
                modules.mkdir(parents=True)
                source = case / "input.elf"
                source.write_bytes(executable_with_dependencies([] if bundled else ["Party"]))
                if bundled:
                    (modules / "engine.prx").write_bytes(named_module(1))
                output = case / ("output.exe" if windows else "output.elf")
                result = subprocess.run([str(relinker), "--registry", "unused-filter=2",
                                         *(["--windows"] if windows else []), str(source), str(output)],
                                        capture_output=True, text=True, timeout=20)
                assert result.returncode == 0, (windows, bundled, result.stdout, result.stderr)
                registry = case / ("output.engine.prx.guest.prx.registry.json" if bundled else "output.registry.json")
                assert json.loads(registry.read_text()) == [], (windows, bundled)
                needed = import_audit.read_needed_registry(import_audit.needed_registry_path(registry))
                assert needed == ["Party.prx"], (windows, bundled, needed)
                records, missing = import_audit.audit(import_audit.read_registry(registry), {}, {}, set(), set(), needed_libraries=needed)
                assert records == [] and missing == {"Party.prx": 0}, (records, missing)
                (modules / "provider.prx").write_bytes(named_module(14))
                result = subprocess.run([str(relinker), "--registry", *(["--windows"] if windows else []),
                                         str(source), str(output)], capture_output=True, text=True, timeout=20)
                assert result.returncode == 0, (windows, bundled, result.stdout, result.stderr)
                needed = import_audit.read_needed_registry(import_audit.needed_registry_path(registry))
                assert needed == [], (windows, bundled, needed)
                assert import_audit.audit([], {}, {}, set(), set(), needed_libraries=needed) == ([], {})
        for windows in (False, True):
            case = work / f"executables-{windows}"
            picker = case / "picker" / "picker.bin"
            picker.parent.mkdir(parents=True)
            modules = case / "sce_module"
            modules.mkdir()
            picker.write_bytes(executable_with_dependencies(["Party"]))
            source = case / "input.elf"
            source.write_bytes(executable_with_dependencies([]))
            output = case / ("output.exe" if windows else "output.elf")
            command = [str(relinker), "--registry", "--executable", "picker/picker.bin",
                       *(["--windows"] if windows else []), str(source), str(output)]
            result = subprocess.run(command, capture_output=True, text=True, timeout=20)
            assert result.returncode == 0, (windows, result.stdout, result.stderr)
            main_needed = case / "output.needed.json"
            picker_needed = case / "picker.picker.bin.needed.json"
            assert import_audit.read_needed_registry(main_needed) == []
            assert import_audit.read_needed_registry(picker_needed) == ["Party.prx"]
            (modules / "provider.prx").write_bytes(named_module(14))
            result = subprocess.run(command, capture_output=True, text=True, timeout=20)
            assert result.returncode == 0, (windows, result.stdout, result.stderr)
            assert import_audit.read_needed_registry(main_needed) == []
            assert import_audit.read_needed_registry(picker_needed) == []
            assert import_audit.read_needed_registry(case / "output.provider.prx.guest.prx.needed.json") == []
    print("Dependency-only library audit tests passed")


if __name__ == "__main__":
    main()
