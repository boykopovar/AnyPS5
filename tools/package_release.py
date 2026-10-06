import argparse
import re
import shutil
import tarfile
import zipfile
from pathlib import Path


def package(platform, build, output, version):
    if not re.fullmatch(r"v[0-9A-Za-z][0-9A-Za-z._-]*", version) or version.endswith("."):
        raise ValueError(f"Invalid release tag for asset filenames: {version}")
    libraries = sorted((build / "core/libs/libs").glob("*.prx"))
    expected = {f"{directory.name}.prx" for directory in Path("core/libs/prx").iterdir() if directory.is_dir()}
    missing = expected - {library.name for library in libraries}
    if missing:
        raise RuntimeError(f"Missing patched libraries: {', '.join(sorted(missing))}")
    executable = "relinker.exe" if platform == "windows" else "relinker"
    files = list(libraries)
    if platform == "windows":
        runtime = Path("C:/winlibs/mingw64/bin")
        files.extend(runtime / name for name in ("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll"))
    binary = build / "core/relinker" / executable
    for file in [*files, binary]:
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
