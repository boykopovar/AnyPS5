import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

from test_game_patches import patch_site, patch_text
from test_guest_intel_trampolines import main_fixture


def main():
    relinker = Path(sys.argv[1]).resolve()
    advisor = Path(__file__).resolve().parents[4] / "tools" / "suggest_patches.py"
    image = bytes(main_fixture())
    address, expected = patch_site(image)
    with tempfile.TemporaryDirectory(prefix="anyps5-patch-advisor-") as directory:
        root = Path(directory)
        source = root / "eboot.bin"
        source.write_bytes(image)
        catalog = root / "catalog"
        catalog.mkdir()
        patch = catalog / "patch.toml"
        patch.write_text(patch_text(address, expected, "flip_bit"))
        metadata = root / "param.json"
        metadata.write_text(json.dumps({"titleId": "TEST00001", "contentVersion": "01.000.000"}))

        def advise(digest, patches=("patch.toml",), *options):
            (catalog / "index.json").write_text(json.dumps({"format": 1, "entries": [{
                "titleId": "TEST00001", "contentVersion": "01.000.000", "patch": name,
                "targets": {"eboot.bin": digest}
            } for name in patches]}))
            return subprocess.run([sys.executable, str(advisor), "--catalog", str(catalog),
                                   "--param-json", str(metadata), "--input", str(source),
                                   "--relinker", f"./{relinker.name}", "--skip-sce-module", *options],
                                  cwd=relinker.parent, capture_output=True, text=True, timeout=30)

        digest = hashlib.sha256(image).hexdigest()
        result = advise(digest)
        assert result.returncode == 0 and f"Recommended: {patch}" in result.stdout, (result.stdout, result.stderr)
        second = catalog / "second patch.toml"
        second.write_text(patch_text(address, expected, "flip_bit"))
        result = advise(digest, ("patch.toml", second.name))
        assert result.returncode == 1 and "multiple verified catalog entries match; use --list-verified to see them" in result.stdout, (result.stdout, result.stderr)
        result = advise(digest, ("patch.toml", second.name), "--list-verified")
        assert result.returncode == 0 and "Verified catalog entries: 2" in result.stdout, (result.stdout, result.stderr)
        assert "Choose one patch file; listed entries are not combined." in result.stdout, (result.stdout, result.stderr)
        assert f"Catalog entry 1: {patch}" in result.stdout and f"Catalog entry 2: {second}" in result.stdout, (result.stdout, result.stderr)
        assert "Recommended:" not in result.stdout, (result.stdout, result.stderr)
        assert source.read_bytes() == image
        broken = catalog / "broken.toml"
        broken.write_text(patch_text(address, expected, "broken").replace('description = "flip one bit"',
                                                                     r'description = "flip \"one\" bit"'))
        result = advise(digest, ("broken.toml", "patch.toml"))
        assert result.returncode == 0 and "relinker error (exit 2)" in result.stdout and f"Recommended: {patch}" in result.stdout, (result.stdout, result.stderr)
        result = advise(digest, ("broken.toml", "patch.toml"), "--list-verified")
        assert result.returncode == 0 and "relinker error (exit 2)" in result.stdout and "Verified catalog entries: 1" in result.stdout, (result.stdout, result.stderr)
        result = advise(digest, ("broken.toml",), "--list-verified")
        assert result.returncode == 2 and "relinker error (exit 2)" in result.stdout, (result.stdout, result.stderr)
        patch.write_text(patch_text(address, expected ^ 1, "wrong_byte"))
        result = advise(digest)
        assert result.returncode == 1 and "rejected by relinker" in result.stdout, (result.stdout, result.stderr)
        result = advise(digest, ("patch.toml",), "--list-verified")
        assert result.returncode == 1 and "rejected by relinker" in result.stdout and "Catalog entry 1:" not in result.stdout, (result.stdout, result.stderr)
        patch.write_text(patch_text(address, expected, "flip_bit"))
        result = advise(hashlib.sha256(b"other build").hexdigest())
        assert result.returncode == 1 and "SHA-256 mismatch" in result.stdout, (result.stdout, result.stderr)


if __name__ == "__main__":
    main()
