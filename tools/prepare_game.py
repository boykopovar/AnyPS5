import argparse
import json
import shutil
import struct
import subprocess
import sys
from pathlib import Path


WINDOWS_RUNTIME = ("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")


def validate_elf(path):
    with path.open("rb") as stream:
        header = stream.read(64)
    if header[:4] != b"\x7fELF":
        raise ValueError(f"Expected an unencrypted ELF, not a console container: {path}")
    if (len(header) != 64 or header[4:7] != b"\x02\x01\x01"
            or struct.unpack_from("<H", header, 18)[0] != 62
            or struct.unpack_from("<I", header, 20)[0] != 1):
        raise ValueError(f"Expected little-endian ELF64 x86-64: {path}")


def native_file(path, platform):
    with path.open("rb") as stream:
        magic = stream.read(4)
    expected = b"MZ" if platform == "windows" else b"\x7fELF"
    if not magic.startswith(expected):
        raise ValueError(f"Expected a library built for {platform}: {path}")


def prepare(args):
    source = args.input.resolve(strict=True)
    assets = (args.assets or source.parent).resolve(strict=True)
    libraries = args.libraries.resolve(strict=True)
    relinker = args.relinker.resolve(strict=True)
    output = args.output.resolve()
    if not assets.is_dir() or not libraries.is_dir() or not relinker.is_file():
        raise ValueError("Assets and libraries must be directories; relinker must be a file")
    if output.exists():
        raise ValueError(f"Output already exists; select a new directory: {output}")
    for protected in (assets, source.parent, libraries):
        if output == protected or protected in output.parents:
            raise ValueError(f"Output must be outside the source and library directories: {output}")
    validate_elf(source)
    module_dirs = [source.parent / name for name in ("sce_module", "sce_modules")
                   if (source.parent / name).exists()]
    if len(module_dirs) != 1 or not module_dirs[0].is_dir():
        raise ValueError("Exactly one sce_module or sce_modules directory must be beside the input ELF")
    modules = module_dirs[0]
    for module in sorted(modules.glob("*.prx")):
        if not module.name.endswith(".guest.prx"):
            validate_elf(module)
    extra_modules = []
    names = {path.name.casefold() if args.platform == "windows" else path.name
             for path in modules.iterdir()}
    for value in args.module:
        if "=" in value:
            name, path = value.split("=", 1)
            path = Path(path).resolve(strict=True)
        else:
            path = Path(value).resolve(strict=True)
            name = path.name
        if not name or name in (".", "..") or any(character in name for character in "/\\:$\r\n"):
            raise ValueError(f"Module name must be a filename: {name}")
        if name.endswith(".guest.prx"):
            raise ValueError(f"Supply an input ELF module, not a converted module: {name}")
        key = name.casefold() if args.platform == "windows" else name
        if key in names:
            raise ValueError(f"Conflicting module filename: {name}")
        names.add(key)
        validate_elf(path)
        extra_modules.append((name, path))
    host_files = sorted(libraries.glob("*.prx"))
    if not host_files:
        raise ValueError(f"No host PRX libraries found: {libraries}")
    for library in host_files:
        native_file(library, args.platform)
    if args.platform == "windows":
        for name in WINDOWS_RUNTIME:
            library = libraries / name
            if not library.is_file() and args.runtime_dll_dir is not None:
                library = args.runtime_dll_dir.resolve(strict=True) / name
            if not library.is_file():
                raise ValueError(f"Missing MinGW runtime {name}; provide --runtime-dll-dir")
            native_file(library, "windows")
            host_files.append(library)
    elif args.runtime_dll_dir is not None:
        raise ValueError("--runtime-dll-dir is only valid for Windows")
    output.mkdir(parents=True)
    report = {"status": "preparing", "platform": args.platform,
              "input": str(source), "assets": str(assets),
              "extra_modules": [{"name": name, "input": str(path)} for name, path in extra_modules]}
    report_path = output / "prepare.json"

    def save_report():
        report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    save_report()
    try:
        conversion_source = source
        if extra_modules:
            stage = output / "relink-input"
            stage.mkdir()
            conversion_source = stage / source.name
            shutil.copy2(source, conversion_source)
            staged_modules = stage / modules.name
            shutil.copytree(modules, staged_modules)
            for name, path in extra_modules:
                shutil.copy2(path, staged_modules / name)

        def ignore_root(directory, names):
            if Path(directory) == assets:
                return set(names) & {source.name, "sce_module", "sce_modules"}
            return set()

        executable = output / ("game.exe" if args.platform == "windows" else "game.elf")
        command = [str(relinker), "--registry", f"unused-filter={args.unused_filter}"]
        if args.platform == "windows":
            command.extend(("--windows", "--windows-diagnostics"))
        if args.to_intel:
            command.append("--to-intel")
        command.extend((str(conversion_source), str(executable)))
        report["command"] = command
        save_report()
        with (output / "conversion.log").open("wb") as log:
            result = subprocess.run(command, cwd=output, stdout=log, stderr=subprocess.STDOUT, check=False)
        report["conversion_exit_code"] = result.returncode
        report["status"] = "converted" if result.returncode == 0 else "conversion_failed"
        if result.returncode == 0:
            if not executable.is_file():
                raise ValueError("Relinker reported success without writing an executable")
            shutil.copytree(assets, output / "app0", ignore=ignore_root, dirs_exist_ok=True)
            (output / "libs").mkdir()
            for library in host_files:
                shutil.copy2(library, output / "libs" / library.name)
            if args.platform == "linux":
                executable.chmod(executable.stat().st_mode | 0o111)
            report["executable"] = str(executable)
        save_report()
        print(f"{report['status']}: {output}")
        print(f"Conversion log: {output / 'conversion.log'}")
        if result.returncode == 0:
            print(f"Run from {output}: {executable.name}")
            print("Game startup and compatibility remain to be tested.")
            return 0
        return 2
    except (OSError, ValueError) as error:
        report.update(status="preparation_failed", error=str(error))
        save_report()
        raise


def main():
    parser = argparse.ArgumentParser(description="Prepare a separate AnyPS5 game test directory")
    parser.add_argument("--input", type=Path, required=True, help="Unencrypted main ELF; modules must be beside it")
    parser.add_argument("--assets", type=Path, help="Game resources; defaults to the input directory")
    parser.add_argument("--output", type=Path, required=True, help="New directory outside the inputs")
    parser.add_argument("--relinker", type=Path, required=True)
    parser.add_argument("--libraries", type=Path, required=True, help="Built host PRX directory or unpacked release libs")
    parser.add_argument("--platform", choices=("windows", "linux"), required=True)
    parser.add_argument("--runtime-dll-dir", type=Path, help="MinGW bin directory if DLLs are absent from --libraries")
    parser.add_argument("--module", action="append", default=[], metavar="[NAME=]ELF",
                        help="Additional input ELF module; repeat as needed, optionally with its imported filename")
    parser.add_argument("--to-intel", action="store_true")
    parser.add_argument("--unused-filter", choices=(0, 1, 2), type=int, default=0)
    args = parser.parse_args()
    try:
        return prepare(args)
    except (OSError, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
