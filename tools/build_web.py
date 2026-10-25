import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import sys
import re
import shutil
import tempfile
import urllib.request
import zipfile

from package_release import WEB_FILES

ROOT = Path(__file__).resolve().parent.parent
MAX_EXPANDED_BYTES = 512 * 1024 * 1024


def copy_sources(output):
    source = ROOT / "demo/web"
    for file in source.iterdir():
        if file.is_file():
            shutil.copy2(file, output / file.name)
    (output / ".nojekyll").touch()


def request(url):
    headers = {"User-Agent": "AnyPS5-Pages"}
    token = os.environ.get("GITHUB_TOKEN")
    if token and url.startswith("https://api.github.com/"):
        headers["Authorization"] = f"Bearer {token}"
    return urllib.request.urlopen(urllib.request.Request(url, headers=headers), timeout=60)


def publish_libraries(output, release=None):
    if release is None:
        with request("https://api.github.com/repos/boykopovar/AnyPS5/releases/latest") as response:
            release = json.load(response)
    tag = release["tag_name"]
    if not re.fullmatch(r"v[0-9A-Za-z][0-9A-Za-z._-]*", tag):
        raise ValueError("Invalid release tag")
    manifest = {"tag": tag, "url": release["html_url"], "platforms": {}}
    destination = output / "libraries"
    destination.mkdir(exist_ok=True)
    for platform in ("windows", "linux"):
        name = f"prx-{platform}-{tag}.zip"
        matches = [asset for asset in release["assets"] if asset["name"] == name]
        if len(matches) != 1:
            raise ValueError(f"Missing or ambiguous release asset: {name}")
        asset = matches[0]
        temporary = destination / f"{platform}.download"
        try:
            digest = hashlib.sha256()
            size = 0
            with request(asset["browser_download_url"]) as response, temporary.open("wb") as target:
                while chunk := response.read(1024 * 1024):
                    size += len(chunk)
                    if size > asset["size"] or size > 512 * 1024 * 1024:
                        raise ValueError("Release archive exceeds expected size")
                    digest.update(chunk)
                    target.write(chunk)
            if size != asset["size"]:
                raise ValueError("Incomplete release archive")
            checksum = digest.hexdigest()
            if asset.get("digest") and asset["digest"] != "sha256:" + checksum:
                raise ValueError("Release archive checksum mismatch")
            with zipfile.ZipFile(temporary) as archive:
                files = archive.infolist()
                if not files or len(files) > 4096:
                    raise ValueError("Invalid library archive")
                names = {}
                for file in files:
                    if (file.orig_filename != file.filename or not file.filename.startswith("libs/")
                            or any(part in ("..", ".", "") for part in file.filename.rstrip("/").split("/")) or "\\" in file.filename):
                        raise ValueError("Invalid library archive layout")
                    name = file.filename.rstrip("/").lower()
                    if name in names:
                        raise ValueError(f"Duplicate library archive path: {file.filename}")
                    names[name] = file.is_dir()
                for name in names:
                    parts = name.split("/")
                    if any(names.get("/".join(parts[:index])) is False for index in range(1, len(parts))):
                        raise ValueError(f"Library archive file is also used as a directory: {name}")
                expanded = sum(file.file_size for file in files)
                if expanded > MAX_EXPANDED_BYTES:
                    raise ValueError("Release archive exceeds maximum expanded size")
                corrupt = archive.testzip()
                if corrupt:
                    raise ValueError(f"Corrupt release entry: {corrupt}")
            path = f"libraries/{platform}-{checksum}.zip"
            temporary.replace(output / path)
            manifest["platforms"][platform] = {"path": path, "size": size, "expandedBytes": expanded, "sha256": checksum}
        finally:
            temporary.unlink(missing_ok=True)
    (output / "release.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Published library assets from {tag}")


def publish_release(output):
    with request("https://api.github.com/repos/boykopovar/AnyPS5/releases/latest") as response:
        release = json.load(response)
    tag = release["tag_name"]
    if not re.fullmatch(r"v[0-9A-Za-z][0-9A-Za-z._-]*", tag) or tag.endswith("."):
        raise ValueError("Invalid release tag")
    name = f"web-{tag}.zip"
    matches = [asset for asset in release["assets"] if asset["name"] == name]
    if len(matches) != 1:
        raise ValueError(f"Missing or ambiguous release asset: {name}; publish a release with the web converter")
    asset = matches[0]
    if not 0 < asset["size"] <= 128 * 1024 * 1024:
        raise ValueError("Invalid web release archive size")
    with request(asset["browser_download_url"]) as response:
        data = response.read(asset["size"] + 1)
    if len(data) != asset["size"]:
        raise ValueError("Incomplete or oversized web release archive")
    checksum = hashlib.sha256(data).hexdigest()
    if asset.get("digest") and asset["digest"] != "sha256:" + checksum:
        raise ValueError("Web release archive checksum mismatch")
    if output.is_symlink() or (output.exists() and not output.is_dir()):
        raise ValueError(f"Release output is not a directory: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=".web-release-", dir=output.parent))
    staging = temporary / "site"
    previous = temporary / "previous"
    published = False
    try:
        staging.mkdir()
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            files = archive.infolist()
            expected = {*WEB_FILES, "web-release.json"}
            if len(files) != len(expected) or {file.filename for file in files} != expected:
                raise ValueError("Invalid web release archive layout")
            if any(file.file_size == 0 for file in files) or sum(file.file_size for file in files) > 128 * 1024 * 1024:
                raise ValueError("Invalid expanded web release archive size")
            metadata = json.loads(archive.read("web-release.json"))
            if (not isinstance(metadata, dict) or metadata.get("tag") != tag
                    or not isinstance(metadata.get("revision"), str)
                    or not re.fullmatch(r"[0-9a-f]{40}", metadata["revision"])):
                raise ValueError("Web converter does not match the selected release")
            for file in files:
                target = staging / file.filename
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(archive.read(file))
        publish_libraries(staging, release)
        manifest = json.loads((staging / "release.json").read_text(encoding="utf-8-sig"))
        manifest["revision"] = metadata["revision"]
        (staging / "release.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        (staging / ".nojekyll").touch()
        if output.exists():
            output.rename(previous)
        try:
            staging.rename(output)
        except OSError:
            if previous.exists():
                try:
                    previous.rename(output)
                except OSError as error:
                    raise RuntimeError(f"Could not restore release output. Previous site is preserved at {previous}") from error
            raise
        published = True
    finally:
        if published or not previous.exists():
            shutil.rmtree(temporary)
    print(f"Published web converter and libraries from {tag} ({metadata['revision']})")


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--release", "--libraries", dest="release", action="store_true",
                        help="Download the matching converter and libraries from the latest release")
    args = parser.parse_args(argv)
    output = args.build.resolve() / "web"
    try:
        if args.release:
            publish_release(output)
            print(output)
            return 0
        for name in ("relinker_wasm.mjs", "relinker_wasm.wasm"):
            if not (output / "native" / name).is_file():
                parser.error(f"Build relinker_wasm first: missing {name}")
        copy_sources(output)
        print(output)
        return 0
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    sys.exit(main())
