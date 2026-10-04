import argparse
import hashlib
import io
import json
import re
import shutil
import subprocess
import tarfile
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
WINDOWS_RUNTIME = ("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")
PORTABLE_DOCUMENTS = (
    ROOT / "README.md",
    ROOT / "LICENSE",
    ROOT / "docs/user/USAGE.md",
    ROOT / "docs/user/INPUT_MAPPING.md",
    ROOT / "docs/user/COMPATIBILITY.md",
)


def expected_libraries():
    expected = {f"{directory.name}.prx" for directory in (ROOT / "core/libs/prx").iterdir() if directory.is_dir()}
    expected.add("libcohtml.Prospero.prx")
    return expected


def compiler_info(build):
    candidates = sorted((build / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake"))
    if not candidates:
        raise RuntimeError(f"CMake C++ compiler metadata was not found under {build}")
    text = candidates[-1].read_text(errors="replace")
    compiler_id = re.search(r'set\(CMAKE_CXX_COMPILER_ID "([^"]+)"\)', text)
    compiler_version = re.search(r'set\(CMAKE_CXX_COMPILER_VERSION "([^"]+)"\)', text)
    if compiler_id is None or compiler_version is None:
        raise RuntimeError(f"Incomplete CMake C++ compiler metadata: {candidates[-1]}")
    return {"id": compiler_id.group(1), "version": compiler_version.group(1)}


def resolve_commit(commit):
    if commit is None:
        commit = subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True, stderr=subprocess.DEVNULL
        ).strip()
    if not re.fullmatch(r"[0-9a-fA-F]{40}", commit):
        raise ValueError(f"Invalid Git commit for build manifest: {commit}")
    return commit.lower()


def portable_manifest(platform, build, version, commit):
    return json.dumps(
        {
            "archive_format": 1,
            "commit": resolve_commit(commit),
            "compiler": compiler_info(build),
            "platform": platform,
            "version": version,
        },
        indent=2,
        sort_keys=True,
    ) + "\n"


def write_portable_zip(path, root_name, files, manifest):
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for source, relative in files:
            archive.write(source, arcname=f"{root_name}/{relative}")
        archive.writestr(f"{root_name}/BUILD-MANIFEST.json", manifest)


def write_portable_tar(path, root_name, files, manifest):
    with tarfile.open(path, "w:gz", compresslevel=9) as archive:
        for source, relative in files:
            info = archive.gettarinfo(str(source), arcname=f"{root_name}/{relative}")
            if relative == "relinker":
                info.mode |= 0o111
            with source.open("rb") as stream:
                archive.addfile(info, stream)
        info = tarfile.TarInfo(f"{root_name}/BUILD-MANIFEST.json")
        data = manifest.encode()
        info.size = len(data)
        info.mode = 0o644
        archive.addfile(info, io.BytesIO(data))


def package(platform, build, output, version, commit=None, runtime_dir=Path("C:/winlibs/mingw64/bin")):
    if not re.fullmatch(r"v[0-9A-Za-z][0-9A-Za-z._-]*", version) or version.endswith("."):
        raise ValueError(f"Invalid release tag for asset filenames: {version}")
    libraries = sorted((build / "core/libs/libs").glob("*.prx"))
    expected = expected_libraries()
    missing = expected - {library.name for library in libraries}
    if missing:
        raise RuntimeError(f"Missing patched libraries: {', '.join(sorted(missing))}")
    executable = "relinker.exe" if platform == "windows" else "relinker"
    files = list(libraries)
    if platform == "windows":
        files.extend(runtime_dir / name for name in WINDOWS_RUNTIME)
    binary = build / "core/relinker" / executable
    for file in [*files, binary, *PORTABLE_DOCUMENTS]:
        if not file.is_file() or file.stat().st_size == 0:
            raise RuntimeError(f"Missing or empty release file: {file}")
    output.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output / f"prx-{platform}-{version}.zip", "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for file in files:
            archive.write(file, arcname=f"libs/{file.name}")
    with tarfile.open(output / f"prx-{platform}-{version}.tar.gz", "w:gz", compresslevel=9) as archive:
        for file in files:
            archive.add(file, arcname=f"libs/{file.name}")
    asset = f"relinker-{version}.exe" if platform == "windows" else f"relinker-{version}"
    shutil.copy2(binary, output / asset)
    root_name = f"AnyPS5-{version}"
    portable_files = [(binary, executable)]
    portable_files.extend((file, f"libs/{file.name}") for file in files)
    portable_files.extend((document, document.relative_to(ROOT).as_posix()) for document in PORTABLE_DOCUMENTS)
    manifest = portable_manifest(platform, build, version, commit)
    if platform == "windows":
        write_portable_zip(output / f"anyps5-{platform}-{version}.zip", root_name, portable_files, manifest)
    else:
        write_portable_tar(output / f"anyps5-{platform}-{version}.tar.gz", root_name, portable_files, manifest)


def collect_docs(source, output):
    documents = sorted(source.rglob("*.md"))
    if not documents:
        raise RuntimeError(f"No Markdown documents found in {source}")
    names = set()
    for document in documents:
        if document.name in names or (output / document.name).exists():
            raise RuntimeError(f"Duplicate release asset name: {document.name}")
        names.add(document.name)
    output.mkdir(parents=True, exist_ok=True)
    for document in documents:
        shutil.copy2(document, output / document.name)


def write_checksums(source, output):
    if not source.is_dir():
        raise RuntimeError(f"Release asset directory was not found: {source}")
    output = output.resolve()
    assets = sorted(path for path in source.iterdir() if path.is_file() and path.resolve() != output)
    if not assets:
        raise RuntimeError(f"No release assets found in {source}")
    lines = []
    for asset in assets:
        digest = hashlib.sha256()
        with asset.open("rb") as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(block)
        lines.append(f"{digest.hexdigest()}  {asset.name}")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines) + "\n", newline="\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--platform", choices=("linux", "windows"))
    parser.add_argument("--build", type=Path)
    parser.add_argument("--version")
    parser.add_argument("--docs", type=Path)
    parser.add_argument("--checksums", type=Path)
    parser.add_argument("--commit")
    parser.add_argument("--runtime-dir", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    package_options = (args.platform, args.build, args.version, args.commit, args.runtime_dir)
    if args.docs is not None or args.checksums is not None:
        if args.docs is not None and args.checksums is not None:
            parser.error("--docs and --checksums cannot be combined")
        if any(value is not None for value in package_options):
            parser.error("--docs/--checksums cannot be combined with package options")
    if args.docs is not None:
        collect_docs(args.docs, args.output)
    elif args.checksums is not None:
        write_checksums(args.checksums, args.output)
    else:
        if args.platform is None or args.build is None or args.version is None:
            parser.error("--platform, --build and --version are required in package mode")
        runtime_dir = args.runtime_dir or Path("C:/winlibs/mingw64/bin")
        package(args.platform, args.build, args.output, args.version, args.commit, runtime_dir)


if __name__ == "__main__":
    main()
