from pathlib import Path
import json
import os
import shutil
import struct
import subprocess
import sys

from guest_import_fixtures import dynamic, fixture_directory
from test_guest_intel_trampolines import main_fixture
from test_guest_needed_modules import executable_with_needed, module_with_needed
from test_guest_symbol_names import imported_symbols, module_symbols
from test_windows_import_modules import consumer, provider


def export(value, main_stack_entry=False):
    image = provider(value, main_stack_entry=main_stack_entry)
    strings = b"\0getaddrinfo\0"
    if main_stack_entry:
        struct.pack_into("<I", image, 0x8b0, len(strings))
        strings += b"EnterGuestMainStack_nid_no_patch\0"
    image[0x800:0x800 + len(strings)] = strings
    struct.pack_into("<Q", image, 0x618, len(strings))
    return image


def add_dependency(image, name):
    size, = struct.unpack_from("<Q", image, 0x618)
    data = name.encode() + b"\0"
    assert size + len(data) < 0x40
    image[0x800 + size:0x800 + size + len(data)] = data
    tags = []
    for offset in range(0x600, 0x800, 16):
        tag, value = struct.unpack_from("<qQ", image, offset)
        if tag == 0:
            break
        tags.append((tag, size + len(data) if tag == 10 else value))
    dynamic(image, 0x600, 176, tags + [(1, size)])
    return image


def convert(relinker, case, target, modules, source=None, global_modules=None):
    directory = case / "cores"
    directory.mkdir(parents=True)
    for name, data in modules.items():
        (directory / name).write_bytes(data)
    for name, data in (global_modules or {}).items():
        bundled = case / "sce_module" / name
        bundled.parent.mkdir(exist_ok=True)
        bundled.write_bytes(data)
    source_path = case / "input.elf"
    source_path.write_bytes(main_fixture() if source is None else source)
    output = case / ("output.exe" if target == "windows" else "output.elf")
    flags = ["--" + target] if target != "linux" else []
    result = subprocess.run([str(relinker), *flags, "--registry", "--module-dir", "cores", str(source_path), str(output)],
                            capture_output=True, text=True, timeout=20)
    return result, output


def run_windows(output, host, kernel=False):
    if os.name != "nt":
        return
    (output.parent / "libs").mkdir()
    shutil.copyfile(host, output.parent / "libs/libc.prx")
    if kernel:
        shutil.copyfile(host, output.parent / "libs/libkernel.prx")
    result = subprocess.run([str(output)], capture_output=True, text=True, timeout=10)
    assert result.returncode == 42, (result.returncode, result.stdout, result.stderr)


def main():
    relinker = Path(sys.argv[1]).resolve()
    target = sys.argv[2]
    with fixture_directory("anyps5-import-scope-") as directory:
        work = Path(directory)
        host = None
        if target == "windows" and os.name == "nt":
            result, output = convert(relinker, work / "host", target, {"host.prx": export(22, main_stack_entry=True)})
            assert result.returncode == 0, (result.stdout, result.stderr)
            host = output.parent / "app0/cores/host.prx.guest.prx"
        for count in (1, 2):
            modules = {"a.prx": export(11), "consumer.prx": consumer("libkernel.prx", "getaddrinfo")}
            if count == 2:
                modules["b.prx"] = export(13)
            result, output = convert(relinker, work / f"unrelated-{count}", target, modules,
                                     executable_with_needed(b"consumer.prx"))
            assert result.returncode == 0, (target, count, result.stdout, result.stderr)
            records = json.loads(output.with_name("output.consumer.prx.guest.prx.registry.json").read_text())
            assert any(record["nid"] == "getaddrinfo" for record in records), records
            if target == "windows":
                run_windows(output, host, kernel=True)
        result, output = convert(relinker, work / "bundled-global", target,
                                 {"consumer.prx": consumer("libkernel.prx", "getaddrinfo")},
                                 executable_with_needed(b"consumer.prx"), {"global.prx": export(22)})
        assert result.returncode == 0, (target, result.stdout, result.stderr)
        records = json.loads(output.with_name("output.consumer.prx.guest.prx.registry.json").read_text())
        assert not any(record["nid"] == "getaddrinfo" for record in records), records
        legacy = consumer("libkernel.prx", "getaddrinfo#A#B", expected=13)
        tags = []
        for offset in range(0x600, 0x800, 16):
            tag, value = struct.unpack_from("<qQ", legacy, offset)
            if tag == 0:
                break
            if tag != 0x61000045:
                tags.append((tag, value))
        dynamic(legacy, 0x600, 176, tags)
        result, output = convert(relinker, work / "legacy-qualified", target,
                                 {"a.prx": export(13), "consumer.prx": legacy},
                                 executable_with_needed(b"consumer.prx"))
        assert result.returncode == 0, (target, result.stdout, result.stderr)
        records = json.loads(output.with_name("output.consumer.prx.guest.prx.registry.json").read_text())
        assert not any(record["nid"] == "getaddrinfo" for record in records), records
        if target == "windows":
            run_windows(output, host)
        for transitive in (False, True):
            modules = {"a.prx": export(22), "b.prx": export(11)}
            modules["consumer.prx"] = consumer("bridge.prx" if transitive else "a.prx", "getaddrinfo")
            if transitive:
                modules["bridge.prx"] = module_with_needed((b"a.prx",), (0,))
            result, output = convert(relinker, work / f"declared-{transitive}", target, modules,
                                     executable_with_needed(b"consumer.prx"))
            if target == "macos":
                assert result.returncode == 2 and "Ambiguous guest import" in result.stderr, result.stderr
                continue
            assert result.returncode == 0, (target, transitive, result.stdout, result.stderr)
            records = json.loads(output.with_name("output.consumer.prx.guest.prx.registry.json").read_text())
            assert not any(record["nid"] == "getaddrinfo" for record in records), records
            if target == "windows":
                run_windows(output, host)
            else:
                artifacts = output.parent / "app0/cores"
                imports = imported_symbols((artifacts / "consumer.prx.guest.prx").read_bytes())
                assert imports & set(module_symbols((artifacts / "a.prx.guest.prx").read_bytes())), imports
                assert not imports & set(module_symbols((artifacts / "b.prx.guest.prx").read_bytes())), imports
        modules = {"a.prx": export(22), "b.prx": export(11),
                   "consumer.prx": add_dependency(consumer("a.prx", "getaddrinfo"), "b.prx")}
        result, output = convert(relinker, work / "ambiguous", target, modules)
        assert result.returncode == 2 and "Ambiguous guest import" in result.stderr, result.stderr
        assert not output.exists()
        modules["consumer.prx"] = consumer("a.prx", "getaddrinfo#A#B")
        result, output = convert(relinker, work / "qualified", target, modules,
                                 executable_with_needed(b"consumer.prx"))
        assert result.returncode == 0, (result.stdout, result.stderr)
        if target == "windows":
            run_windows(output, host)
    print(f"Guest import scope tests passed ({target})")


if __name__ == "__main__":
    main()
