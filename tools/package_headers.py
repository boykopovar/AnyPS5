import os
from pathlib import Path, PurePosixPath
import re
import stat
import tarfile
import tempfile
import zipfile

from nid_names import CPP_TOKEN


SYSTEM_HEADERS = frozenset("""algorithm any array atomic barrier bit bitset cassert ccomplex cctype cerrno cfenv cfloat charconv chrono
cinttypes ciso646 climits clocale cmath codecvt compare complex concepts condition_variable coroutine
csetjmp csignal cstdalign cstdarg cstdbool cstddef cstdint cstdio cstdlib cstring ctgmath ctime cuchar cwchar cwctype
deque exception execution filesystem format forward_list fstream functional future initializer_list iomanip
ios iosfwd iostream istream iterator latch limits list locale map memory memory_resource mutex new numbers
numeric optional ostream queue random ranges ratio regex scoped_allocator semaphore set shared_mutex source_location
span sstream stack stdexcept stop_token streambuf string string_view strstream syncstream system_error thread tuple
type_traits typeindex typeinfo unordered_map unordered_set utility valarray variant vector version
assert.h complex.h ctype.h errno.h fenv.h float.h inttypes.h iso646.h limits.h locale.h math.h setjmp.h signal.h
stdalign.h stdarg.h stdatomic.h stdbool.h stddef.h stdint.h stdio.h stdlib.h stdnoreturn.h string.h tgmath.h
threads.h time.h uchar.h wchar.h wctype.h
cxxabi.h dispatch/dispatch.h io.h link.h semaphore.h stdio_ext.h sys/uio.h unistd.h xmmintrin.h
""".split())


def resolved_path(path):
    path = Path(path)
    resolved = Path(os.path.realpath(path))
    if resolved.is_symlink():
        raise RuntimeError(f"Symbolic link loop: {path}")
    return resolved


def public_headers(root):
    libs = resolved_path(root / "core/libs")
    directories = sorted(path for path in (libs / "prx").iterdir() if path.is_dir() and not path.name.startswith("."))
    if not directories:
        raise RuntimeError("No public library headers found")
    pending = [path / (path.name + ".hpp") for path in directories]
    visited = set()
    while pending:
        header = resolved_path(pending.pop())
        if not header.is_relative_to(libs):
            raise RuntimeError(f"Public header is outside core/libs: {header}")
        if header in visited:
            continue
        if not header.is_file():
            raise RuntimeError(f"Missing public header: {header}")
        visited.add(header)
        text = header.read_text(encoding="utf-8").replace("\\\n", "").replace("\\\r\n", "")
        text = CPP_TOKEN.sub(lambda match: re.sub(r"[^\n]", " ", match.group()) if match.group("comment") or match.group("raw") else match.group(), text)
        for line in re.findall(r"^[ \t]*#[ \t]*include\b([^\n]*)", text, re.M):
            match = re.fullmatch(r'\s*(?:"([^"\n]+)"|<([^>\n]+)>)\s*', line)
            if not match:
                raise RuntimeError(f"Non-literal public include in {header}: {line.strip()}")
            quoted, angled = match.groups()
            name = quoted or angled
            if "\\" in name or ":" in name or PurePosixPath(name).is_absolute() or "3rdparty" in PurePosixPath(name).parts:
                raise RuntimeError(f"Forbidden public include in {header}: {name}")
            search = (header.parent / name, libs / name) if quoted else (libs / name,)
            candidates = set()
            for path in search:
                if path.is_symlink():
                    candidates.add(resolved_path(path))
                elif path.is_file():
                    candidates.add(resolved_path(path))
            if len(candidates) > 1:
                raise RuntimeError(f"Ambiguous public include in {header}: {name}")
            if candidates:
                pending.append(candidates.pop())
            elif quoted or name not in SYSTEM_HEADERS:
                raise RuntimeError(f"Missing project or unsupported external include in {header}: {name}")
    return sorted(visited)


def package(root, output, version):
    if not re.fullmatch(r"v[0-9A-Za-z][0-9A-Za-z._-]*", version) or version.endswith("."):
        raise ValueError(f"Invalid release tag for asset filenames: {version}")
    libs = resolved_path(root / "core/libs")
    headers = public_headers(root)
    license_file = root / "LICENSE"
    if not license_file.is_file() or license_file.stat().st_size == 0:
        raise RuntimeError("Missing project license")
    entries = [(path, "include/anyps5/" + path.relative_to(libs).as_posix()) for path in headers]
    entries.append((license_file, "LICENSE"))
    names = [f"prx-headers-{version}.zip", f"prx-headers-{version}.tar.gz"]
    for name in names:
        path = output / name
        if path.exists() and not path.is_file():
            raise RuntimeError(f"Release destination is not a file: {path}")
        if os.name == "nt" and os.path.lexists(path):
            attributes = path.lstat().st_file_attributes
            if attributes & (stat.FILE_ATTRIBUTE_DIRECTORY | stat.FILE_ATTRIBUTE_READONLY):
                raise RuntimeError(f"Release destination is a directory or read-only: {path}")
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".headers-", dir=output) as temporary:
        staging = Path(temporary)
        with zipfile.ZipFile(staging / names[0], "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9, strict_timestamps=False) as archive:
            for source, destination in entries:
                archive.write(source, destination)
        with tarfile.open(staging / names[1], "w:gz", dereference=True) as archive:
            for source, destination in entries:
                archive.add(source, destination)
        for name in names:
            (staging / name).replace(output / name)
    print(f"Packaged {len(headers)} public headers and recursive dependencies")
