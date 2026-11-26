import datetime
import gzip
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import tarfile
import tempfile
import zipfile


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
cxxabi.h dispatch/dispatch.h io.h link.h semaphore.h stdio_ext.h sys/stat.h sys/uio.h unistd.h xmmintrin.h
""".split())

_NON_CODE_TRIGGER = re.compile(r'//|/\*|"|\'')
_RAW_BODY = re.compile(r'"(?P<delimiter>[^\s()\\]{0,16})\([\s\S]*?\)(?P=delimiter)"')
_STRING_BODY = re.compile(r'"(?:\\[\s\S]|[^"\\])*"')
_CHAR_BODY = re.compile(r"'(?:\\[\s\S]|[^'\\])*'")
_CODE_TOKEN = re.compile(r"(?:[0-9]|\.[0-9])[A-Za-z0-9_.']*|[A-Za-z_][A-Za-z0-9_]*|\S")
_PRE_TOKEN_CHARS = frozenset("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.'")
_RAW_PREFIXES = frozenset(("R", "u8R", "uR", "UR", "LR"))
_INCLUDE_DIRECTIVE = re.compile(r"^[ \t]*(?:#|%:)[ \t]*(include(?:_next)?)\b([^\n]*)", re.M)
_INCLUDE_TARGET = re.compile(r'\s*(?:"([^"\n]+)"|<([^>\n]+)>)\s*')


def _token_before(text, index, floor):
    scan_start = index
    while scan_start > floor and text[scan_start - 1] in _PRE_TOKEN_CHARS:
        scan_start -= 1
    if scan_start == index:
        return None, None, index
    last_start = index
    last_token = None
    for token in _CODE_TOKEN.finditer(text, scan_start):
        if token.start() >= index:
            break
        if token.end() > index:
            return token.start(), token.group(), token.end()
        last_start = token.start()
        last_token = token.group()
        if token.end() == index:
            return last_start, last_token, index
    return None, None, index


def _blank_span(text, start, end):
    span = text[start:end]
    if "\n" not in span:
        return " " * (end - start)
    return "\n".join(" " * len(line) for line in span.split("\n"))


def _strip_comments_and_raw(text):
    out = []
    pos = 0
    cursor = 0
    floor = 0
    no_block_close = False
    length = len(text)
    while cursor < length:
        trigger = _NON_CODE_TRIGGER.search(text, cursor)
        if trigger is None:
            break
        index = trigger.start()
        token = trigger.group()
        if token == "//":
            end = text.find("\n", index + 2)
            if end == -1:
                end = length
            out.append(text[pos:index])
            out.append(" " * (end - index))
            pos = cursor = floor = end
        elif token == "/*":
            if no_block_close:
                cursor = floor = index + 2
                continue
            close = text.find("*/", index + 2)
            if close == -1:
                no_block_close = True
                cursor = floor = index + 2
                continue
            end = close + 2
            out.append(text[pos:index])
            out.append(_blank_span(text, index, end))
            pos = cursor = floor = end
        elif token == '"':
            tok_start, tok_text, tok_end = _token_before(text, index, floor)
            if tok_end > index:
                cursor = floor = tok_end
                continue
            if tok_text in _RAW_PREFIXES:
                raw_match = _RAW_BODY.match(text, index)
                if raw_match is not None:
                    end = raw_match.end()
                    out.append(text[pos:tok_start])
                    out.append(_blank_span(text, tok_start, end))
                    pos = cursor = floor = end
                    continue
            str_match = _STRING_BODY.match(text, index)
            if str_match is not None:
                cursor = floor = str_match.end()
            else:
                cursor = floor = index + 1
        else:
            _, _, tok_end = _token_before(text, index, floor)
            if tok_end > index:
                cursor = floor = tok_end
                continue
            char_match = _CHAR_BODY.match(text, index)
            if char_match is not None:
                cursor = floor = char_match.end()
            else:
                cursor = floor = index + 1
    if not out:
        return text
    out.append(text[pos:])
    return "".join(out)


def resolved_path(path):
    path = Path(path)
    resolved = Path(os.path.realpath(path))
    if resolved.is_symlink():
        raise RuntimeError(f"Symbolic link loop: {path}")
    return resolved


def public_headers(root):
    resolved_cache = {}
    exists_cache = {}
    seen_sources = set()

    def resolve_cached(path):
        key = os.fspath(path)
        cached = resolved_cache.get(key)
        if cached is None:
            cached = resolved_path(path)
            resolved_cache[key] = cached
        return cached

    libs = resolve_cached(root / "core/libs")
    directories = sorted(path for path in (libs / "prx").iterdir() if path.is_dir() and not path.name.startswith("."))
    if not directories:
        raise RuntimeError("No public library headers found")
    pending = [path / (path.name + ".hpp") for path in directories]
    visited = set()
    while pending:
        source = pending.pop()
        source_key = os.fspath(source)
        if source_key in seen_sources:
            continue
        header = Path(os.path.abspath(source))
        target = resolve_cached(source)
        if resolve_cached(header) != target:
            raise RuntimeError(f"Public header path changes when normalized: {source}")
        if not header.is_relative_to(libs) or not target.is_relative_to(libs):
            raise RuntimeError(f"Public header is outside core/libs: {header}")
        if header in visited:
            seen_sources.add(source_key)
            continue
        if not header.is_file():
            raise RuntimeError(f"Missing public header: {header}")
        visited.add(header)
        seen_sources.add(source_key)
        text = header.read_text(encoding="utf-8-sig")
        if "\\\n" in text:
            text = text.replace("\\\n", "")
        if "\\\r\n" in text:
            text = text.replace("\\\r\n", "")
        if "#" not in text and "%:" not in text:
            continue
        text = _strip_comments_and_raw(text)
        for keyword, line in _INCLUDE_DIRECTIVE.findall(text):
            if keyword == "include_next":
                raise RuntimeError(f"Unsupported include_next in {header}: {line.strip()}")
            match = _INCLUDE_TARGET.fullmatch(line)
            if not match:
                raise RuntimeError(f"Non-literal public include in {header}: {line.strip()}")
            quoted, angled = match.groups()
            name = quoted or angled
            if "\\" in name or ":" in name or PurePosixPath(name).is_absolute() or "3rdparty" in PurePosixPath(name).parts:
                raise RuntimeError(f"Forbidden public include in {header}: {name}")
            search = (header.parent / name, libs / name) if quoted else (libs / name,)
            candidates = {}
            for path in search:
                key = os.fspath(path)
                exists = exists_cache.get(key)
                if exists is None:
                    exists = path.is_symlink() or path.is_file()
                    exists_cache[key] = exists
                if exists:
                    candidates.setdefault(resolve_cached(path), path)
            if len(candidates) > 1:
                raise RuntimeError(f"Ambiguous public include in {header}: {name}")
            if candidates:
                pending.append(next(iter(candidates.values())))
            elif quoted or name not in SYSTEM_HEADERS:
                raise RuntimeError(f"Missing project or unsupported external include in {header}: {name}")
    return sorted(visited)


def source_date_epoch():
    text = os.environ.get("SOURCE_DATE_EPOCH")
    if text is None:
        return None
    if not re.fullmatch(r"[0-9]+", text):
        raise ValueError("SOURCE_DATE_EPOCH must be an ASCII decimal integer between 0 and 4294967295")
    digits = text.lstrip("0") or "0"
    if len(digits) > 10 or int(digits) > 4294967295:
        raise ValueError("SOURCE_DATE_EPOCH must be an ASCII decimal integer between 0 and 4294967295")
    return int(digits)


def write_header_archives(entries, staging, names, epoch):
    with zipfile.ZipFile(staging / names[0], "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9, strict_timestamps=False) as archive:
        for source, destination in entries:
            if epoch is None:
                archive.write(source, destination)
            else:
                timestamp = datetime.datetime(1970, 1, 1, tzinfo=datetime.timezone.utc) + datetime.timedelta(seconds=max(epoch, 315532800))
                info = zipfile.ZipInfo(destination, timestamp.timetuple()[:6])
                info.create_system = 3
                info.external_attr = (stat.S_IFREG | 0o644) << 16
                archive.writestr(info, source.read_bytes(), compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)
    if epoch is None:
        with tarfile.open(staging / names[1], "w:gz", dereference=True) as archive:
            for source, destination in entries:
                archive.add(source, destination)
    else:
        with (staging / names[1]).open("wb") as target, gzip.GzipFile(filename="", mode="wb", fileobj=target, mtime=epoch, compresslevel=9) as compressed:
            with tarfile.open(mode="w", fileobj=compressed, dereference=True) as archive:
                for source, destination in entries:
                    info = archive.gettarinfo(source, arcname=destination)
                    info.uid = info.gid = 0
                    info.uname = info.gname = ""
                    info.mode = 0o644
                    info.mtime = epoch
                    info.pax_headers = {}
                    with source.open("rb") as content:
                        archive.addfile(info, content)


def package(root, output, version):
    if not re.fullmatch(r"v[0-9A-Za-z][0-9A-Za-z._-]*", version) or version.endswith("."):
        raise ValueError(f"Invalid release tag for asset filenames: {version}")
    epoch = source_date_epoch()
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
    staging = Path(tempfile.mkdtemp(prefix=".headers-", dir=output))
    preserve_staging = False
    try:
        write_header_archives(entries, staging, names, epoch)
        previous = staging / "previous"
        previous.mkdir()
        try:
            for name in names:
                destination = output / name
                if destination.exists() or destination.is_symlink():
                    destination.replace(previous / name)
            for name in names:
                (staging / name).replace(output / name)
        except BaseException as promotion_error:
            preserve_staging = True
            backed_up = [name for name in names if os.path.lexists(previous / name)]
            replaced = [name for name in names if not os.path.lexists(staging / name)]
            restore_error = None
            unrestored = []
            leftovers = []
            for name in reversed(replaced):
                try:
                    (output / name).unlink(missing_ok=True)
                except OSError as error:
                    restore_error = restore_error or error
                    leftovers.append(name)
            for name in reversed(backed_up):
                try:
                    (previous / name).replace(output / name)
                    if name in leftovers:
                        leftovers.remove(name)
                except OSError as error:
                    restore_error = restore_error or error
                    unrestored.append(name)
            if restore_error is not None:
                message = f"Could not restore public header release archives after {promotion_error}"
                if unrestored:
                    preserve_staging = True
                    message += f"; unrestored backups {', '.join(unrestored)} retained at {previous}"
                if leftovers:
                    preserve_staging = True
                    paths = ", ".join(str(output / name) for name in leftovers)
                    message += f"; promoted archives could not be removed and remain at {paths}; remaining staged archives retained at {staging}"
                raise RuntimeError(message) from restore_error
            preserve_staging = False
            raise
    finally:
        if not preserve_staging:
            shutil.rmtree(staging)
    print(f"Packaged {len(headers)} public headers and recursive dependencies")
