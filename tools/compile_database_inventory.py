import argparse
import json
import ntpath
import posixpath
import stat
import sys
from pathlib import Path

from check_compile import compilation_database


NINJA_LOG_HEADERS = {"# ninja log v5", "# ninja log v6", "# ninja log v7"}


def output_key(paths, directory, output):
    return paths.normcase(paths.normpath(paths.join(directory, output)))


def latest_ninja_outputs(log, paths, build_directory):
    if not stat.S_ISREG(log.stat().st_mode):
        raise ValueError(f"{log}: Ninja log must be a regular file")
    with log.open(encoding="utf-8-sig") as source:
        if source.readline().rstrip("\n") not in NINJA_LOG_HEADERS:
            raise ValueError(f"{log}: unsupported Ninja build log header")
        outputs = {}
        for line_number, line in enumerate(source, 2):
            try:
                start, end, _, output, _ = line.rstrip("\n").split("\t")
                start, end = int(start), int(end)
                if not output or not 0 <= start <= end:
                    raise ValueError
            except ValueError:
                raise ValueError(f"{log}:{line_number}: invalid Ninja build log record") from None
            outputs[output_key(paths, build_directory, output)] = {
                "output": output, "duration_ms": end - start,
            }
    return outputs


def add_ninja_timings(units, log, build_directory):
    directories = [directory for directory, _ in units]
    candidates = directories + ([build_directory] if build_directory else [])
    paths = ntpath if any(ntpath.splitdrive(directory)[0] for directory in candidates) else posixpath
    if build_directory is None:
        try:
            build_directory = paths.commonpath(directories) if directories else "."
        except ValueError:
            raise ValueError("compile directories have no common root; specify --ninja-build-dir") from None
    outputs = latest_ninja_outputs(log, paths, build_directory)
    compile_outputs = set()
    entries_without_output = 0
    for (directory, _), records in units.items():
        for record in records:
            output = record.get("output")
            if not output:
                entries_without_output += 1
                continue
            key = output_key(paths, directory, output)
            compile_outputs.add(key)
            if key in outputs:
                record["ninja_duration_ms"] = outputs[key]["duration_ms"]
    matched = compile_outputs & outputs.keys()
    return {
        "compile_outputs": len(compile_outputs),
        "matched_compile_outputs": len(matched),
        "entries_without_output": entries_without_output,
        "summed_compile_duration_ms": sum(outputs[key]["duration_ms"] for key in matched),
        "other_outputs": [outputs[key] for key in sorted(outputs.keys() - compile_outputs)],
    }


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Group the raw records of a saved compilation database without invoking a compiler or build tool")
    parser.add_argument("database", type=Path)
    parser.add_argument("--ninja-log", type=Path, help="add timings from this saved Ninja build log")
    parser.add_argument("--ninja-build-dir", help="original Ninja working directory for relative log paths")
    args = parser.parse_args(argv)
    if args.ninja_build_dir is not None and args.ninja_log is None:
        parser.error("--ninja-build-dir requires --ninja-log")
    try:
        if not stat.S_ISREG(args.database.stat().st_mode):
            raise ValueError(f"{args.database}: compilation database must be a regular file")
        entries = compilation_database(args.database.read_text(encoding="utf-8-sig"), args.database)
        units = {}
        for index, item in enumerate(entries):
            record = {"arguments": item["arguments"]} if "arguments" in item else {"command": item["command"]}
            if "output" in item:
                if not isinstance(item["output"], str):
                    raise ValueError(f"{args.database}: entry {index}: output must be a string")
                record["output"] = item["output"]
            units.setdefault((item["directory"], item["file"]), []).append(record)
        timing = None
        if args.ninja_log is not None:
            timing = add_ninja_timings(units, args.ninja_log, args.ninja_build_dir)
    except (OSError, ValueError, RecursionError) as error:
        parser.error(str(error))
    report = {"entries": len(entries), "units": [
        {"directory": directory, "file": file, "variants": len(records), "records": records}
        for (directory, file), records in sorted(units.items())]}
    if timing is not None:
        report["ninja_timings"] = timing
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
