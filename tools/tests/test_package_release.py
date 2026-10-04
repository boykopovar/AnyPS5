import hashlib
import importlib.util
import json
import stat
import tarfile
import tempfile
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("package_release", ROOT / "tools/package_release.py")
PACKAGE_RELEASE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE_RELEASE)
VERSION = "v1.2.3-test"
COMMIT = "0123456789abcdef0123456789abcdef01234567"


def fixture(root, platform):
    build = root / "build"
    libraries = build / "core/libs/libs"
    libraries.mkdir(parents=True)
    for name in PACKAGE_RELEASE.expected_libraries():
        (libraries / name).write_bytes(b"prx")
    executable = build / "core/relinker" / ("relinker.exe" if platform == "windows" else "relinker")
    executable.parent.mkdir(parents=True)
    executable.write_bytes(b"relinker")
    executable.chmod(executable.stat().st_mode | stat.S_IXUSR)
    compiler = build / "CMakeFiles/4.0.0/CMakeCXXCompiler.cmake"
    compiler.parent.mkdir(parents=True)
    compiler.write_text(
        'set(CMAKE_CXX_COMPILER_ID "GNU")\nset(CMAKE_CXX_COMPILER_VERSION "15.2.0")\n'
    )
    runtime = root / "runtime"
    runtime.mkdir()
    for name in PACKAGE_RELEASE.WINDOWS_RUNTIME:
        (runtime / name).write_bytes(b"runtime")
    return build, runtime


def verify_manifest(data, platform):
    manifest = json.loads(data)
    assert manifest == {
        "archive_format": 1,
        "commit": COMMIT,
        "compiler": {"id": "GNU", "version": "15.2.0"},
        "platform": platform,
        "version": VERSION,
    }


def verify_windows(root):
    build, runtime = fixture(root, "windows")
    output = root / "dist"
    PACKAGE_RELEASE.package("windows", build, output, VERSION, COMMIT, runtime)
    portable = output / f"anyps5-windows-{VERSION}.zip"
    assert portable.is_file()
    with zipfile.ZipFile(portable) as archive:
        prefix = f"AnyPS5-{VERSION}/"
        names = set(archive.namelist())
        assert prefix + "relinker.exe" in names
        assert prefix + "README.md" in names
        assert prefix + "LICENSE" in names
        assert prefix + "docs/user/USAGE.md" in names
        assert prefix + "BUILD-MANIFEST.json" in names
        for name in PACKAGE_RELEASE.WINDOWS_RUNTIME:
            assert prefix + "libs/" + name in names
        for name in PACKAGE_RELEASE.expected_libraries():
            assert prefix + "libs/" + name in names
        verify_manifest(archive.read(prefix + "BUILD-MANIFEST.json"), "windows")
    assert (output / f"prx-windows-{VERSION}.zip").is_file()
    assert (output / f"prx-windows-{VERSION}.tar.gz").is_file()
    assert (output / f"relinker-{VERSION}.exe").read_bytes() == b"relinker"
    return output


def verify_linux(root):
    build, runtime = fixture(root, "linux")
    output = root / "dist"
    PACKAGE_RELEASE.package("linux", build, output, VERSION, COMMIT, runtime)
    portable = output / f"anyps5-linux-{VERSION}.tar.gz"
    assert portable.is_file()
    with tarfile.open(portable) as archive:
        prefix = f"AnyPS5-{VERSION}/"
        names = set(archive.getnames())
        assert prefix + "relinker" in names
        assert prefix + "README.md" in names
        assert prefix + "LICENSE" in names
        assert prefix + "docs/user/USAGE.md" in names
        assert prefix + "BUILD-MANIFEST.json" in names
        assert archive.getmember(prefix + "relinker").mode & stat.S_IXUSR
        for name in PACKAGE_RELEASE.expected_libraries():
            assert prefix + "libs/" + name in names
        verify_manifest(archive.extractfile(prefix + "BUILD-MANIFEST.json").read(), "linux")
    assert (output / f"prx-linux-{VERSION}.zip").is_file()
    assert (output / f"prx-linux-{VERSION}.tar.gz").is_file()
    assert (output / f"relinker-{VERSION}").read_bytes() == b"relinker"
    return output


def verify_checksums(output):
    checksums = output / "SHA256SUMS.txt"
    PACKAGE_RELEASE.write_checksums(output, checksums)
    lines = checksums.read_text().splitlines()
    assert lines
    assert all(not line.endswith("  SHA256SUMS.txt") for line in lines)
    assert [line.split("  ", 1)[1] for line in lines] == sorted(
        path.name for path in output.iterdir() if path.is_file() and path != checksums
    )
    for line in lines:
        digest, name = line.split("  ", 1)
        assert hashlib.sha256((output / name).read_bytes()).hexdigest() == digest


def main():
    with tempfile.TemporaryDirectory(prefix="anyps5-package-release-") as directory:
        root = Path(directory)
        output = verify_windows(root / "windows")
        verify_checksums(output)
        output = verify_linux(root / "linux")
        verify_checksums(output)
    print("Release packaging tests passed")


if __name__ == "__main__":
    main()
