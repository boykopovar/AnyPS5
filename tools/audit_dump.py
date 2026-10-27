import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import import_audit
import unwrap_self

DESCRIPTION = """Audit the imports of an extracted title before it is launched. Unwraps the dump's eboot.bin and the modules
it ships (sce_module/, sce_modules/ and prx/, Unity's Media/Modules/ and Media/Plugins/, and .prx files beside
eboot.bin) into a work directory, converts them with relinker --registry, and runs tools/import_audit.py on the
registries with the unwrapped modules as --modules. The dump is only read; the source/ and output/ directories of a
reused --work directory are emptied first. Exit code: that of import_audit.py, or 2
when unwrapping or conversion fails."""

STANDARD_DIRECTORIES = ("sce_module", "sce_modules", "prx")
EXTRA_DIRECTORIES = ("Media/Modules", "Media/Plugins")
MODULE_SUFFIXES = (".prx", ".sprx")
ELF_MAGIC = b"\x7fELF"


def stage(source, destination):
    try:
        unwrap_self.unwrap(source, destination)
        return "unwrapped"
    except unwrap_self.Refused:
        with open(source, "rb") as file:
            if file.read(4) != ELF_MAGIC:
                raise
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, destination)
        return "copied"


def modules_of(directory):
    return sorted(path for path in directory.iterdir() if (path.is_file() or path.is_symlink()) and path.suffix.lower() in MODULE_SUFFIXES)


def prepare(dump, source, log=print):
    stage(dump / "eboot.bin", source / "eboot.elf")
    module_directories = []
    extra = []
    for relative in STANDARD_DIRECTORIES + EXTRA_DIRECTORIES:
        directory = dump / relative
        if not directory.is_dir():
            continue
        modules = modules_of(directory)
        if not modules:
            continue
        for module in modules:
            stage(module, source / relative / module.name)
        module_directories.append(source / relative)
        if relative in EXTRA_DIRECTORIES:
            extra.append(relative)
    beside = modules_of(dump)
    if beside:
        for module in beside:
            stage(module, source / module.name)
        module_directories.append(source)
        extra.append(".")
    log(f"staged eboot.bin and {sum(len(modules_of(directory)) for directory in module_directories)} modules")
    return module_directories, extra


def convert(relinker, source, output, extra, windows):
    command = [str(relinker), "--registry"]
    if windows:
        command.append("--windows")
    for relative in extra:
        command += ["--module-dir", relative]
    command += [str(source / "eboot.elf"), str(output)]
    return subprocess.run(command, capture_output=True, text=True)


def comparison_path(path, links=frozenset()):
    for prefix in (*reversed(path.parents), path):
        if not prefix.is_symlink():
            continue
        identity = prefix.parent.resolve() / prefix.name
        if identity in links:
            raise RuntimeError("symbolic link cycle")
        target = prefix.readlink()
        destination = target if target.is_absolute() else prefix.parent / target
        resolved_target = comparison_path(destination, links | {identity})
        return comparison_path(resolved_target / path.relative_to(prefix), links)
    resolved = path.resolve()
    drive = resolved.drive
    if drive.casefold().startswith("\\\\?\\unc\\"):
        return type(resolved)("\\\\" + str(resolved)[8:])
    if len(drive) == 6 and drive.startswith("\\\\?\\") and drive.endswith(":"):
        return type(resolved)(str(resolved)[4:])
    return resolved


def report_overlaps_dump(report, dump):
    destination = comparison_path(report)
    dump_path = comparison_path(dump)
    if report.absolute().is_relative_to(dump.absolute()) or destination.is_relative_to(dump_path) or comparison_path(report.parent).is_relative_to(dump_path):
        return True
    exists = report.exists()
    for path in dump.rglob("*"):
        if path.is_symlink() and path.is_dir() and destination.is_relative_to(comparison_path(path)):
            return True
        if exists and path.is_file() and report.samefile(path):
            return True
    return False


def main(argv=None):
    parser = argparse.ArgumentParser(description=DESCRIPTION, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("dump", help="the title's extracted app0 directory, holding eboot.bin")
    parser.add_argument("--relinker", required=True, help="the relinker executable")
    parser.add_argument("--libs", action="append", required=True, help="built .prx directory, as for import_audit.py (repeatable)")
    parser.add_argument("--work", help="directory for the unwrapped files and registries (default: a temporary one, removed afterwards)")
    parser.add_argument("--windows", action="store_true", help="convert for Windows, as the title would be run there")
    parser.add_argument("--json", help="also write import_audit.py's JSON report to this file outside the input dump")
    args = parser.parse_args(argv)
    dump = Path(args.dump)
    if not (dump / "eboot.bin").is_file():
        parser.error(f"{dump}: no eboot.bin")
    relinker = Path(args.relinker).resolve()
    if not relinker.is_file():
        parser.error(f"{args.relinker}: no such relinker")
    dump_path = comparison_path(dump)
    if args.json:
        report = Path(args.json)
        try:
            overlaps = report_overlaps_dump(report, dump)
        except (OSError, RuntimeError) as error:
            parser.error(f"{report}: cannot check JSON output against the input dump: {error}")
        if overlaps:
            parser.error(f"{report}: JSON output overlaps the input dump")
    with tempfile.TemporaryDirectory() as temporary:
        work = Path(args.work) if args.work else Path(temporary)
        source, output = work / "source", work / "output"
        for generated in (source, output):
            generated_path = comparison_path(generated)
            if dump_path.is_relative_to(generated_path) or generated_path.is_relative_to(dump_path):
                parser.error(f"{generated}: work directory overlaps the input dump")
        for generated in (source, output):
            try:
                if generated.is_symlink():
                    generated.unlink()
                elif generated.exists():
                    shutil.rmtree(generated)
            except OSError as error:
                print(f"FAIL: cannot empty {generated}: {error}", file=sys.stderr)
                return 2
            if generated.exists() or generated.is_symlink():
                print(f"FAIL: cannot empty {generated}: it still exists", file=sys.stderr)
                return 2
        output.mkdir(parents=True, exist_ok=True)
        try:
            module_directories, extra = prepare(dump, source)
        except (unwrap_self.Refused, OSError) as error:
            print(f"FAIL: unwrapping {dump}: {error}", file=sys.stderr)
            return 2
        result = convert(relinker, source, output / ("app.exe" if args.windows else "app.elf"), extra, args.windows)
        if result.returncode != 0:
            print(f"FAIL: relinker exited with {result.returncode}", file=sys.stderr)
            print((result.stderr or result.stdout).strip(), file=sys.stderr)
            return 2
        registries = sorted(str(path) for path in output.rglob("*.registry.json"))
        audit = registries + [option for library in args.libs for option in ("--libs", library)]
        audit += [option for directory in module_directories for option in ("--modules", str(directory))]
        if args.json:
            audit += ["--json", args.json]
        return import_audit.main(audit)


if __name__ == "__main__":
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    sys.exit(main())
