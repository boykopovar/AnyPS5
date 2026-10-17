import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import urllib.request
import zipfile

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


def publish_libraries(output):
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
                    if not file.filename.startswith("libs/") or any(part in ("..", ".", "") for part in file.filename.rstrip("/").split("/")) or "\\" in file.filename:
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


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--libraries", action="store_true")
    args = parser.parse_args()
    output = args.build.resolve() / "web"
    for name in ("relinker_wasm.mjs", "relinker_wasm.wasm"):
        if not (output / "native" / name).is_file():
            raise ValueError(f"Build relinker_wasm first: missing {name}")
    copy_sources(output)
    if args.libraries:
        publish_libraries(output)
    print(output)


if __name__ == "__main__":
    main()
