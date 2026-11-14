from pathlib import Path
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

from test_guest_intel_trampolines import main_fixture as fixture


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def png(width, height):
    rows = b"".join(b"\0" + bytes((15, 65, 200, 255)) * width for _ in range(height))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def unescape(value):
    result, index = "", 0
    while index < len(value):
        if value[index] == "\\" and index + 1 < len(value):
            result += {"s": " ", "n": "\n", "t": "\t", "r": "\r", "\\": "\\"}[value[index + 1]]
            index += 2
        else:
            result += value[index]
            index += 1
    return result


def unquote(value):
    assert value.startswith('"') and value.endswith('"'), value
    result, index = "", 1
    while index < len(value) - 1:
        if value[index] == "\\":
            assert value[index + 1] in '"`$\\', value
            index += 1
        elif value[index] == "%":
            assert value[index + 1] == "%", value
            index += 1
        result += value[index]
        index += 1
    return result


def entry(path):
    lines = path.read_text(encoding="utf-8").splitlines()
    assert lines[0] == "[Desktop Entry]", lines
    return {key: value.lstrip(" \t") for key, value in (line.split("=", 1) for line in lines[1:])}


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-desktop-") as directory:
        work = Path(directory)
        app = work / "app with spaces"
        (app / "sce_sys").mkdir(parents=True)
        source = app / "eboot.bin"
        source.write_bytes(fixture())

        def relink(name, *options):
            output = work / name
            result = subprocess.run([str(relinker), "--skip-sce-module", *options, str(source), str(output)],
                                    cwd=work, capture_output=True, text=True, timeout=20)
            desktop, icon = Path(f"{output}.desktop"), Path(f"{output}.png")
            return result, output, desktop, icon

        result, output, desktop, icon = relink("no-icon")
        assert result.returncode == 0, (result.stdout, result.stderr)
        assert output.exists() and not desktop.exists() and not icon.exists()

        image = png(64, 64)
        (app / "sce_sys" / "icon0.png").write_bytes(image)
        (app / "sce_sys" / "param.json").write_text(json.dumps(
            {"titleId": "PPSA00000", "defaultLanguage": "en-US",
             "localizedParameters": {"en-US": {"titleName": "Sample Title"}}}))

        result, output, desktop, icon = relink("linux")
        assert result.returncode == 0, (result.stdout, result.stderr)
        assert icon.read_bytes() == image
        fields = entry(desktop)
        assert fields["Name"] == "Sample Title", fields
        assert unquote(unescape(fields["Exec"])) == str(output), fields
        assert unescape(fields["Path"]) == str(work) and unescape(fields["Icon"]) == str(icon), fields
        assert os.name == "nt" or desktop.stat().st_mode & 0o111
        validator = shutil.which("desktop-file-validate")
        if validator:
            checked = subprocess.run([validator, str(desktop)], capture_output=True, text=True)
            assert checked.returncode == 0 and not checked.stdout.strip(), checked.stdout

        (app / "sce_sys" / "param.json").unlink()
        result, output, desktop, icon = relink("  leading space")
        assert result.returncode == 0, (result.stdout, result.stderr)
        fields = entry(desktop)
        assert unescape(fields["Name"]) == output.stem, fields
        assert unquote(unescape(fields["Exec"])) == str(output), fields

        for name, option in (("macos", "--macos"), ("windows.exe", "--windows")):
            result, output, desktop, icon = relink(name, option)
            assert result.returncode == 0, (name, result.stdout, result.stderr)
            assert output.exists() and not desktop.exists() and not icon.exists(), name

        (app / "sce_sys" / "icon0.png").write_bytes(b"not a PNG")
        result, output, desktop, icon = relink("bad-icon")
        assert result.returncode != 0, (result.stdout, result.stderr)
        assert not output.exists() and not desktop.exists() and not icon.exists()
        assert "Invalid icon PNG" in result.stderr, result.stderr


if __name__ == "__main__":
    main()
