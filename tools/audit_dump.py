import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import import_audit
import unwrap_self

DESCRIPTION = """Audit the imports of an extracted title before it is launched. Unwraps the dump's eboot.bin and the modules
it ships (sce_module/, sce_modules/ and prx/, Unity's Media/Modules/ and Media/Plugins/, and module files beside
eboot.bin) into a work directory, converts them with relinker --registry, and runs tools/import_audit.py on the
registries with the unwrapped modules as --modules. A file found by its header rather than a module suffix that is
not a shared module after unwrapping, such as a dumper's eboot.bin.esbak backup, is skipped with a message. So is
one the relinker refuses for a ps5-payload-dev SDK syscall stub, such as a payload for the console's ELF loader: the
conversion then runs again without it. The dump
is only read; the source/ and output/ directories of a reused --work directory are emptied first. Exit code: that of import_audit.py, or 2
when unwrapping or conversion fails."""

STANDARD_DIRECTORIES = ("sce_module", "sce_modules", "prx")
EXTRA_DIRECTORIES = ("Media/Modules", "Media/Plugins")
MODULE_SUFFIXES = import_audit.MODULE_FILE_SUFFIXES
ELF_MAGIC = b"\x7fELF"
MODULE_MAGICS = (ELF_MAGIC, unwrap_self.SELF_MAGIC.to_bytes(4, "little"),
                 unwrap_self.SELF_MAGIC_KERNEL.to_bytes(4, "little"))
SHARED_MODULE_TYPES = (3, 0xFE18)
PAYLOAD_REFUSAL = "it is a ps5-payload-dev SDK syscall stub"
INPUT_PREFIX = "Input: "


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


def elf_type(path):
    with open(path, "rb") as file:
        header = file.read(18)
    return int.from_bytes(header[16:18], "little") if len(header) == 18 else None


def stage_modules(modules, source, log):
    staged = []
    for module in modules:
        destination = source / module.name
        stage(module, destination)
        if module.suffix.lower() not in MODULE_SUFFIXES:
            kind = elf_type(destination)
            if kind not in SHARED_MODULE_TYPES:
                destination.unlink()
                described = "unknown" if kind is None else f"0x{kind:x}"
                log(f"skipped {module.name}: not a shared module (ELF type {described})")
                continue
        staged.append(module)
    return staged


def modules_of(directory):
    modules = []
    for path in directory.iterdir():
        if not (path.is_file() or path.is_symlink()):
            continue
        if path.suffix.lower() in MODULE_SUFFIXES:
            modules.append(path)
        elif path.is_file():
            with path.open("rb") as file:
                if file.read(4) in MODULE_MAGICS:
                    modules.append(path)
    return sorted(modules)


def prepare(dump, source, log=print):
    for module in modules_of(dump):
        if source / module.name == source / "eboot.elf" and not module.samefile(dump / "eboot.bin"):
            raise unwrap_self.Refused(f"{module}: module name collides with the staged executable eboot.elf")
    stage(dump / "eboot.bin", source / "eboot.elf")
    module_directories = []
    extra = []
    count = 0
    for relative in STANDARD_DIRECTORIES + EXTRA_DIRECTORIES:
        directory = dump / relative
        if not directory.is_dir():
            continue
        modules = stage_modules(modules_of(directory), source / relative, log)
        if not modules:
            if relative in STANDARD_DIRECTORIES:
                (source / relative).mkdir(parents=True, exist_ok=True)
            continue
        count += len(modules)
        module_directories.append(source / relative)
        if relative in EXTRA_DIRECTORIES:
            extra.append(relative)
    beside = stage_modules([module for module in modules_of(dump) if not module.samefile(dump / "eboot.bin")], source, log)
    if beside:
        count += len(beside)
        module_directories.append(source)
        extra.append(".")
    log(f"staged eboot.bin and {count} modules")
    return module_directories, extra


def audit_directories(directories, source):
    audited = []
    for directory in directories:
        if directory == source:
            directory = Path(tempfile.mkdtemp(prefix="modules-", dir=source))
            for module in sorted(source.iterdir()):
                if module.is_file() and module.name != "eboot.elf":
                    try:
                        os.link(module, directory / module.name)
                    except OSError:
                        shutil.copyfile(module, directory / module.name)
        audited.append(directory)
    return audited


def refused_payload(diagnostic, source):
    lines = diagnostic.splitlines()
    for index, line in enumerate(lines):
        if index == 0 or not line.startswith(INPUT_PREFIX) or PAYLOAD_REFUSAL not in lines[index - 1]:
            continue
        refused = Path(line[len(INPUT_PREFIX):].strip())
        if refused.suffix.lower() in MODULE_SUFFIXES:
            return None
        try:
            wanted = comparison_path(refused)
            for staged in source.rglob("*"):
                if staged.is_file() and staged != source / "eboot.elf" and comparison_path(staged) == wanted:
                    return staged, lines[index - 1].removeprefix("FAIL: ").strip()
        except (OSError, RuntimeError):
            return None
    return None


def remaining(module_directories, extra, source):
    kept = [directory for directory in module_directories
            if any(path.is_file() and path != source / "eboot.elf" for path in directory.iterdir())]
    names = {"." if directory == source else directory.relative_to(source).as_posix() for directory in kept}
    return kept, [relative for relative in extra if relative in names]


def convert(relinker, source, output, extra, windows):
    command = [str(relinker), "--registry"]
    if windows:
        command.append("--windows")
    for relative in extra:
        command += ["--module-dir", relative]
    command += [str(source / "eboot.elf"), str(output)]
    return subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace")


def comparison_path(path, links=frozenset()):
    for prefix in (*reversed(path.parents), path):
        if not prefix.is_symlink():
            continue
        identity = (prefix.parent.resolve() / prefix.name).parts
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
        if path.is_dir() and destination.is_relative_to(comparison_path(path)):
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
    if not Path(args.relinker).is_file():
        parser.error(f"{args.relinker}: no such relinker")
    relinker = Path(args.relinker).resolve()
    dump_path = comparison_path(dump)
    try:
        dump_inputs = [dump / "eboot.bin", *modules_of(dump)]
        dump_directories = []
        for relative in STANDARD_DIRECTORIES + EXTRA_DIRECTORIES:
            directory = dump / relative
            if directory.is_dir():
                dump_directories.append(directory)
                dump_inputs.extend((directory, *modules_of(directory)))
    except (OSError, RuntimeError) as error:
        parser.error(f"{dump}: cannot enumerate dump inputs before cleanup: {error}")
    report_locations = ()
    if args.json:
        report = Path(args.json)
        try:
            overlaps = report_overlaps_dump(report, dump)
            report_path = comparison_path(report)
            report_locations = (Path(os.path.abspath(report)), report_path, comparison_path(report.parent) / report.name)
            relinker_alias = report_path == comparison_path(relinker) or (report.exists() and report.samefile(relinker))
        except (OSError, RuntimeError) as error:
            parser.error(f"{report}: cannot check JSON output against the input dump: {error}")
        if relinker_alias:
            parser.error(f"{report}: JSON output would overwrite the relinker")
        if overlaps:
            parser.error(f"{report}: JSON output overlaps the input dump")
        if report.is_dir():
            parser.error(f"{report}: JSON output is a directory")
        if not report.resolve().parent.is_dir():
            parser.error(f"{report.resolve().parent}: no such directory")
    with tempfile.TemporaryDirectory() as temporary:
        work = Path(args.work) if args.work else Path(temporary)
        source, output = work / "source", work / "output"
        for generated in (source, output):
            try:
                generated_path = comparison_path(generated)
            except (OSError, RuntimeError) as error:
                parser.error(f"{generated}: cannot check work directory against the input dump: {error}")
            generated_spelling = Path(os.path.abspath(generated))
            if any(location.is_relative_to(generated_spelling) or location.is_relative_to(generated_path)
                   for location in report_locations):
                parser.error(f"{report}: JSON output overlaps a work directory that will be cleared: {generated}")
            if dump_path.is_relative_to(generated_path) or generated_path.is_relative_to(dump_path):
                parser.error(f"{generated}: work directory overlaps the input dump")
            for input_path in (Path(args.relinker), *(Path(library) for library in args.libs), *dump_inputs):
                try:
                    resolved = comparison_path(input_path)
                except (OSError, RuntimeError) as error:
                    parser.error(f"{input_path}: cannot check work directory against input: {error}")
                supplied = Path(os.path.abspath(input_path))
                if (supplied.is_relative_to(generated_spelling) or resolved.is_relative_to(generated_path)
                        or input_path in dump_directories and generated_path.is_relative_to(resolved)):
                    parser.error(f"{generated}: work directory contains input {input_path}")
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
        try:
            output.mkdir(parents=True, exist_ok=True)
        except OSError as error:
            print(f"FAIL: cannot create {output}: {error}", file=sys.stderr)
            return 2
        try:
            module_directories, extra = prepare(dump, source)
        except (unwrap_self.Refused, OSError) as error:
            print(f"FAIL: unwrapping {dump}: {error}", file=sys.stderr)
            return 2
        while True:
            try:
                result = convert(relinker, source, output / ("app.exe" if args.windows else "app.elf"), extra, args.windows)
            except OSError as error:
                print(f"FAIL: cannot run relinker {relinker}: {error}", file=sys.stderr)
                return 2
            if result.returncode == 0:
                break
            diagnostic = (result.stderr or result.stdout).strip()
            refused = refused_payload(diagnostic, source)
            if refused is None:
                print(f"FAIL: relinker exited with {result.returncode}", file=sys.stderr)
                print(diagnostic, file=sys.stderr)
                return 2
            payload, reason = refused
            try:
                payload.unlink()
                shutil.rmtree(output)
                output.mkdir(parents=True)
            except OSError as error:
                print(f"FAIL: cannot repeat the conversion without {payload.name}: {error}", file=sys.stderr)
                return 2
            print(f"skipped {payload.name}: {reason}")
            module_directories, extra = remaining(module_directories, extra, source)
        registries = sorted(str(path) for path in output.rglob("*.registry.json"))
        audit = registries + [option for library in args.libs for option in ("--libs", library)]
        try:
            audited = audit_directories(module_directories, source)
        except OSError as error:
            print(f"FAIL: cannot gather the modules beside eboot.bin: {error}", file=sys.stderr)
            return 2
        audit += [option for directory in audited for option in ("--modules", str(directory))]
        if args.json:
            audit += ["--json", args.json]
        return import_audit.main(audit)


if __name__ == "__main__":
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    sys.exit(main())
