import argparse
import os
import re
import shutil
import stat
import tarfile
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def windows_runtime(build):
    for line in (build / "CMakeCache.txt").read_text(encoding="utf-8").splitlines():
        if line.startswith("CMAKE_CXX_COMPILER:"):
            compiler = line.partition("=")[2]
            if compiler and not compiler.endswith("-NOTFOUND"):
                return Path(compiler).parent
    raise RuntimeError(f"CMAKE_CXX_COMPILER is not configured in {build / 'CMakeCache.txt'}")


def tracy_runtime(platform, build):
    cache = build / "CMakeCache.txt"
    if not cache.is_file():
        return []
    lines = cache.read_text(encoding="utf-8").splitlines()
    enabled = next((line.partition("=")[2].upper() for line in lines if line.startswith("APS5_ENABLE_TRACY:")), "OFF")
    if enabled in ("", "0", "OFF", "NO", "FALSE", "N", "IGNORE", "NOTFOUND") or enabled.endswith("-NOTFOUND"):
        return []
    pattern = "libTracyClient.dll" if platform == "windows" else "libTracyClient.so.*"
    directory = build / "core/libs/libs"
    clients = sorted(directory.glob(pattern))
    if len(clients) != 1:
        raise RuntimeError(f"Missing or ambiguous Tracy runtime for APS5_ENABLE_TRACY: {directory / pattern}")
    return clients


def package(platform, build, output, version):
    if not re.fullmatch(r"v[0-9A-Za-z][0-9A-Za-z._-]*", version) or version.endswith("."):
        raise ValueError(f"Invalid release tag for asset filenames: {version}")
    libraries = sorted((build / "core/libs/libs").glob("*.prx"))
    expected = {f"{directory.name}.prx" for directory in (ROOT / "core/libs/prx").iterdir() if directory.is_dir() and not directory.name.startswith(".")}
    missing = expected - {library.name for library in libraries}
    if missing:
        raise RuntimeError(f"Missing patched libraries: {', '.join(sorted(missing))}")
    executable = "relinker.exe" if platform == "windows" else "relinker"
    files = [library for library in libraries if library.name in expected]
    if platform == "windows":
        runtime = windows_runtime(build)
        files.extend(runtime / name for name in ("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll"))
        files.extend(build / "core/libs/libs" / name for name in ("mimalloc.dll", "mimalloc-redirect.dll"))
    files.extend(tracy_runtime(platform, build))
    binary = build / "core/relinker" / executable
    for file in [*files, binary]:
        if not file.is_file() or file.stat().st_size == 0:
            raise RuntimeError(f"Missing or empty release file: {file}")
    asset = f"relinker-{version}.exe" if platform == "windows" else f"relinker-{version}"
    names = (f"prx-{platform}-{version}.zip", f"prx-{platform}-{version}.tar.gz", asset)
    for name in names:
        destination = output / name
        if destination.exists() and not destination.is_file():
            raise RuntimeError(f"Release destination is not a file: {destination}")
        if os.name == "nt" and os.path.lexists(destination):
            attributes = destination.lstat().st_file_attributes
            if attributes & stat.FILE_ATTRIBUTE_DIRECTORY:
                raise RuntimeError(f"Release destination is not a file: {destination}")
            if attributes & stat.FILE_ATTRIBUTE_READONLY:
                raise RuntimeError(f"Release destination is read-only: {destination}")
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".release-", dir=output) as temporary:
        staging = Path(temporary)
        with zipfile.ZipFile(staging / f"prx-{platform}-{version}.zip", "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for file in files:
                archive.write(file, arcname=f"libs/{file.name}")
        with tarfile.open(staging / f"prx-{platform}-{version}.tar.gz", "w:gz", compresslevel=9, dereference=True) as archive:
            for file in files:
                archive.add(file, arcname=f"libs/{file.name}")
        shutil.copy2(binary, staging / asset)
        for file in sorted(staging.iterdir()):
            file.replace(output / file.name)


def collect_docs(source, output):
    documents = sorted(document for document in source.rglob("*.md") if document.is_file())
    if not documents:
        raise RuntimeError(f"No Markdown documents found in {source}")
    names = {file.name.casefold() for file in output.iterdir()} if output.exists() else set()
    for document in documents:
        if document.name.casefold() in names:
            raise RuntimeError(f"Duplicate release asset name: {document.name}")
        names.add(document.name.casefold())
    output.mkdir(parents=True, exist_ok=True)
    for document in documents:
        shutil.copy2(document, output / document.name)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--platform", choices=("linux", "windows"))
    parser.add_argument("--build", type=Path)
    parser.add_argument("--version")
    parser.add_argument("--docs", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if args.docs is not None:
        if args.platform is not None or args.build is not None or args.version is not None:
            parser.error("--docs cannot be combined with --platform, --build or --version")
        collect_docs(args.docs, args.output)
    else:
        if args.platform is None or args.build is None or args.version is None:
            parser.error("--platform, --build and --version are required when --docs is not specified")
        package(args.platform, args.build, args.output, args.version)


if __name__ == "__main__":
    main()
