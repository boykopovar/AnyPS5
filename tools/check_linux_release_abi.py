import argparse
import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path, PurePosixPath


LIMITS = {"GLIBC": (2, 38), "GLIBCXX": (3, 4, 31)}
RUNTIME = re.compile(r"\.(?:prx|so(?:\..+)?)$")
PROVIDER = re.compile(r"\bFile:\s+(.+?)\s+Cnt:")
REQUIREMENT = re.compile(r"\bName:\s+(\S+)")
NUMERIC = re.compile(r"[0-9]+(?:\.[0-9]+)+")


def version_needs(output):
    needs = False
    provider = None
    for line in output.splitlines():
        if line.startswith("Version ") and " section " in line:
            needs = line.startswith("Version needs section ")
            provider = None
        if not needs:
            continue
        library = PROVIDER.search(line)
        if library:
            provider = library.group(1)
        name = REQUIREMENT.search(line)
        if name:
            if provider is None:
                raise ValueError("readelf reported a version requirement without a provider")
            yield provider, name.group(1)


def incompatible_versions(output):
    for provider, version in version_needs(output):
        family, separator, required = version.partition("_")
        limit = LIMITS.get(family)
        if not separator or limit is None or version == "GLIBC_ABI_DT_RELR":
            continue
        if not NUMERIC.fullmatch(required) or tuple(map(int, required.split("."))) > limit:
            maximum = family + "_" + ".".join(map(str, limit))
            yield f"requires {version} from {provider} (release baseline: {maximum})"


def read_version_info(path):
    result = subprocess.run(
        ["readelf", "--version-info", "--wide", str(path)],
        capture_output=True, text=True, encoding="utf-8", errors="replace",
        env={**os.environ, "LC_ALL": "C"},
    )
    if result.returncode:
        raise ValueError(result.stderr.strip() or f"readelf exited {result.returncode}")
    return result.stdout


def check_archive(path, scratch):
    count = 0
    issues = []
    with tarfile.open(path, "r|gz") as archive:
        for member in archive:
            parts = PurePosixPath(member.name).parts
            if len(parts) != 2 or parts[0] != "libs" or not RUNTIME.search(parts[1]):
                continue
            if not member.isfile():
                raise ValueError(f"{path}:{member.name}: expected a regular runtime file")
            with archive.extractfile(member) as source, scratch.open("wb") as output:
                shutil.copyfileobj(source, output)
            try:
                findings = list(incompatible_versions(read_version_info(scratch)))
            except ValueError as error:
                raise ValueError(f"{path}:{member.name}: {error}") from error
            issues.extend(f"{path}:{member.name}: {finding}" for finding in findings)
            count += 1
    if not count:
        raise ValueError(f"{path}: no runtime libraries under libs/")
    return count, issues


def main(argv=None):
    parser = argparse.ArgumentParser(description="Check Linux release archives against the supported glibc and libstdc++ ABI")
    parser.add_argument("archives", nargs="+", type=Path, help="patched and unpatched Linux .tar.gz release archives")
    args = parser.parse_args(argv)
    failed = False
    try:
        with tempfile.TemporaryDirectory() as directory:
            scratch = Path(directory) / "runtime.elf"
            for archive in args.archives:
                count, issues = check_archive(archive, scratch)
                for issue in issues:
                    print(issue, file=sys.stderr)
                print(f"{archive}: checked {count} runtime libraries")
                failed = failed or bool(issues)
    except (OSError, ValueError, tarfile.TarError) as error:
        parser.error(str(error))
    return int(failed)


if __name__ == "__main__":
    sys.exit(main())
