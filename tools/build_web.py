import argparse
import ast
import hashlib
import io
import json
import os
from pathlib import Path
import sys
import re
import shutil
import subprocess
import tempfile
import urllib.request
import zipfile

from package_release import WEB_FILES

ROOT = Path(__file__).resolve().parent.parent
MAX_EXPANDED_BYTES = 512 * 1024 * 1024


def release_web_files(revision):
    try:
        result = subprocess.run(
            ["git", "-C", str(ROOT), "show", f"{revision}:tools/package_release.py"],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
    except OSError as error:
        raise ValueError(f"Cannot resolve release revision: {revision}") from error
    if result.returncode != 0:
        raise ValueError(f"Cannot resolve release revision: {revision}")
    try:
        tree = ast.parse(result.stdout)
        for node in tree.body:
            value = None
            if isinstance(node, ast.Assign):
                for target in node.targets:
                    if isinstance(target, ast.Name) and target.id == "WEB_FILES":
                        value = ast.literal_eval(node.value)
                        break
            elif isinstance(node, ast.AnnAssign):
                if isinstance(node.target, ast.Name) and node.target.id == "WEB_FILES" and node.value is not None:
                    value = ast.literal_eval(node.value)
            if value is not None:
                if (isinstance(value, (tuple, list)) and value
                        and all(isinstance(item, str) and item and "\\" not in item
                                and not item.startswith("/")
                                and not any(part in ("..", ".", "") for part in item.split("/"))
                                for item in value)
                        and len(value) == len(set(value))):
                    return set(value)
                raise ValueError(f"Invalid web release definition in revision: {revision}")
    except (ValueError, SyntaxError, TypeError) as error:
        raise ValueError(f"Invalid web release definition in revision: {revision}") from error
    raise ValueError(f"Invalid web release definition in revision: {revision}")


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


def release_metadata(release):
    if not isinstance(release, dict):
        raise ValueError("release metadata must be a JSON object")
    for field in ("tag_name", "html_url"):
        if not isinstance(release.get(field), str) or not release[field]:
            raise ValueError(f"release metadata {field} must be nonempty text")
    assets = release.get("assets")
    if not isinstance(assets, list) or any(not isinstance(asset, dict) or not isinstance(asset.get("name"), str)
                                         or not asset["name"] for asset in assets):
        raise ValueError("release metadata assets must be a list of named asset objects")
    return release


def asset_metadata(asset, maximum):
    name = asset["name"]
    if type(asset.get("size")) is not int or not 0 < asset["size"] <= maximum:
        raise ValueError(f"Invalid release asset {name}: archive size must be a positive integer no greater than {maximum}")
    if not isinstance(asset.get("browser_download_url"), str) or not asset["browser_download_url"]:
        raise ValueError(f"Invalid release asset {name}: browser_download_url must be nonempty text")
    if asset.get("digest") is not None and not isinstance(asset["digest"], str):
        raise ValueError(f"Invalid release asset {name}: digest must be text or null")
    return asset


def library_assets(release):
    assets = {}
    for platform in ("windows", "linux"):
        name = f"prx-{platform}-{release['tag_name']}.zip"
        matches = [asset for asset in release["assets"] if asset["name"] == name]
        if len(matches) != 1:
            raise ValueError(f"Missing or ambiguous release asset: {name}")
        assets[platform] = asset_metadata(matches[0], 512 * 1024 * 1024)
    return assets


def load_release(response):
    try:
        return json.load(response)
    except RecursionError as error:
        raise ValueError(f"Invalid release metadata: {error}") from error


def publish_libraries(output, release=None):
    if release is None:
        with request("https://api.github.com/repos/boykopovar/AnyPS5/releases/latest") as response:
            release = load_release(response)
    release = release_metadata(release)
    tag = release["tag_name"]
    if not re.fullmatch(r"v[0-9A-Za-z][0-9A-Za-z._-]*", tag):
        raise ValueError("Invalid release tag")
    manifest = {"tag": tag, "url": release["html_url"], "platforms": {}}
    assets = library_assets(release)
    destination = output / "libraries"
    destination.mkdir(exist_ok=True)
    for platform in ("windows", "linux"):
        asset = assets[platform]
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
                if any(file.flag_bits & 1 for file in files):
                    raise ValueError(f"Encrypted release archive {asset['name']} is not supported")
                if any(file.compress_type not in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED) for file in files):
                    raise ValueError("Library ZIP archive compression must be Store or Deflate")
                if any((file.external_attr >> 16) & 0o170000 not in (0, 0o100000, 0o040000) for file in files):
                    raise ValueError("Library ZIP links and special files are not supported")
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
        except (zipfile.BadZipFile, NotImplementedError) as error:
            raise ValueError(f"Invalid release archive {asset['name']}: {error}") from error
        finally:
            temporary.unlink(missing_ok=True)
    (output / "release.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Published library assets from {tag}")


def publish_release(output):
    with request("https://api.github.com/repos/boykopovar/AnyPS5/releases/latest") as response:
        release = load_release(response)
    release = release_metadata(release)
    tag = release["tag_name"]
    if not re.fullmatch(r"v[0-9A-Za-z][0-9A-Za-z._-]*", tag) or tag.endswith("."):
        raise ValueError("Invalid release tag")
    name = f"web-{tag}.zip"
    matches = [asset for asset in release["assets"] if asset["name"] == name]
    if len(matches) != 1:
        raise ValueError(f"Missing or ambiguous release asset: {name}; publish a release with the web converter")
    asset = asset_metadata(matches[0], 128 * 1024 * 1024)
    library_assets(release)
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
            if any(file.flag_bits & 1 for file in files):
                raise ValueError(f"Encrypted release archive {asset['name']} is not supported")
            if any(file.file_size == 0 for file in files) or sum(file.file_size for file in files) > 128 * 1024 * 1024:
                raise ValueError("Invalid expanded web release archive size")
            filenames = {file.filename for file in files}
            if len(files) != len(filenames) or "web-release.json" not in filenames:
                raise ValueError("Invalid web release archive layout")
            try:
                metadata = load_release(archive.open("web-release.json"))
            except (json.JSONDecodeError, UnicodeDecodeError) as error:
                raise ValueError("Invalid web release metadata") from error
            if (not isinstance(metadata, dict) or metadata.get("tag") != tag
                    or not isinstance(metadata.get("revision"), str)
                    or not re.fullmatch(r"[0-9a-f]{40}", metadata["revision"])):
                raise ValueError("Web converter does not match the selected release")
            expected_files = release_web_files(metadata["revision"])
            expected = {*expected_files, "web-release.json"}
            if len(files) != len(expected) or filenames != expected:
                raise ValueError("Invalid web release archive layout")
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
    except (zipfile.BadZipFile, NotImplementedError) as error:
        raise ValueError(f"Invalid release archive {asset['name']}: {error}") from error
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
            if (output / "native" / name).stat().st_size == 0:
                parser.error(f"Build relinker_wasm first: empty {name}")
        copy_sources(output)
        print(output)
        return 0
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    sys.exit(main())
