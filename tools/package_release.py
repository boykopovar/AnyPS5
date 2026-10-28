import argparse
import json
import os
import re
import shutil
import stat
import sys
import tarfile
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WEB_FILES = ("index.html", "app.css", "app.mjs", "archive.mjs", "batch.mjs", "failure-report.mjs", "output-directory.mjs", "worker.mjs",
             "native/relinker_wasm.mjs", "native/relinker_wasm.wasm")


def windows_runtime(build):
    cache = build / "CMakeCache.txt"
    if not cache.is_file():
        raise RuntimeError(f"Missing CMakeCache.txt in build directory: {build}")
    for line in cache.read_text(encoding="utf-8-sig").splitlines():
        if line.startswith("CMAKE_CXX_COMPILER:"):
            compiler = line.partition("=")[2]
            if compiler and not compiler.endswith("-NOTFOUND"):
                return Path(compiler).parent
    raise RuntimeError(f"CMAKE_CXX_COMPILER is not configured in {cache}")


def tracy_runtime(platform, build):
    cache = build / "CMakeCache.txt"
    if not cache.is_file():
        return []
    lines = cache.read_text(encoding="utf-8-sig").splitlines()
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
    licenses = []
    if "libSceAjm.native.prx" in expected:
        licenses.append((ROOT / "3rdparty/vgmstream/COPYING", "libs/licenses/vgmstream.txt"))
    if "libSceVorbisDec.prx" in expected:
        licenses.append((ROOT / "3rdparty/ogg/COPYING", "libs/licenses/ogg.txt"))
        licenses.append((ROOT / "3rdparty/vorbis/COPYING", "libs/licenses/vorbis.txt"))
    if expected & {"libSceOpusDec.prx", "libSceOpusCeltDec.prx"}:
        licenses.append((ROOT / "3rdparty/opus/COPYING", "libs/licenses/opus.txt"))
    if platform == "windows":
        runtime = windows_runtime(build)
        files.extend(runtime / name for name in ("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll"))
        files.extend(build / "core/libs/libs" / name for name in ("mimalloc.dll", "mimalloc-redirect.dll"))
    files.extend(tracy_runtime(platform, build))
    binary = build / "core/relinker" / executable
    for file in [*files, binary, *(source for source, _ in licenses)]:
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
            for source, destination in licenses:
                archive.write(source, arcname=destination)
        with tarfile.open(staging / f"prx-{platform}-{version}.tar.gz", "w:gz", compresslevel=9, dereference=True) as archive:
            for file in files:
                archive.add(file, arcname=f"libs/{file.name}")
            for source, destination in licenses:
                archive.add(source, arcname=destination)
        shutil.copy2(binary, staging / asset)
        for file in sorted(staging.iterdir()):
            file.replace(output / file.name)


def collect_docs(source, output):
    if not source.is_dir():
        raise RuntimeError(f"Documentation source is not a directory: {source}")
    documents = sorted(document for document in source.rglob("*.md") if document.is_file())
    if not documents:
        raise RuntimeError(f"No Markdown documents found in {source}")
    names = {file.name.casefold() for file in output.iterdir()} if output.exists() else set()
    for document in documents:
        if document.name.casefold() in names:
            raise RuntimeError(f"Duplicate release asset name: {document.name}")
        names.add(document.name.casefold())
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".docs-", dir=output) as temporary:
        staging = Path(temporary)
        for document in documents:
            shutil.copy2(document, staging / document.name)
        for document in documents:
            (staging / document.name).replace(output / document.name)


def package_web(build, output, version, revision):
    if not re.fullmatch(r"v[0-9A-Za-z][0-9A-Za-z._-]*", version) or version.endswith("."):
        raise ValueError(f"Invalid release tag for asset filenames: {version}")
    if not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise ValueError("Web release revision must be a full commit SHA")
    source = build / "web"
    for name in WEB_FILES:
        file = source / name
        if not file.is_file() or file.stat().st_size == 0:
            raise RuntimeError(f"Missing or empty web release file: {file}")
    name = f"web-{version}.zip"
    destination = output / name
    if destination.exists() and not destination.is_file():
        raise RuntimeError(f"Release destination is not a file: {destination}")
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".release-", dir=output) as temporary:
        asset = Path(temporary) / name
        with zipfile.ZipFile(asset, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for file in WEB_FILES:
                archive.write(source / file, arcname=file)
            archive.writestr("web-release.json", json.dumps({"tag": version, "revision": revision}) + "\n")
        asset.replace(destination)


def main(argv=None):
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser()
    parser.add_argument("--platform", choices=("linux", "windows", "web"))
    parser.add_argument("--build", type=Path)
    parser.add_argument("--version")
    parser.add_argument("--revision")
    parser.add_argument("--docs", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args(argv)
    try:
        if args.docs is not None:
            if args.platform is not None or args.build is not None or args.version is not None or args.revision is not None:
                parser.error("--docs cannot be combined with --platform, --build, --version or --revision")
            collect_docs(args.docs, args.output)
        else:
            if args.platform is None or args.build is None or args.version is None:
                parser.error("--platform, --build and --version are required when --docs is not specified")
            if args.platform == "web":
                if args.revision is None:
                    parser.error("--revision is required for --platform web")
                package_web(args.build, args.output, args.version, args.revision)
            else:
                if args.revision is not None:
                    parser.error("--revision requires --platform web")
                package(args.platform, args.build, args.output, args.version)
    except (RuntimeError, ValueError, OSError) as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    sys.exit(main())
