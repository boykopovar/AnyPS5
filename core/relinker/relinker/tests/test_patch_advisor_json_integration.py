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
    advisor = Path(__file__).resolve().parents[4] / "tools/suggest_patches.py"
    image = bytes(main_fixture())
    address, expected = patch_site(image)
    with tempfile.TemporaryDirectory(prefix="patch-advisor-json-") as directory:
        root = Path(directory)
        source = root / "eboot.bin"
        source.write_bytes(image)
        catalog = root / "catalog"
        catalog.mkdir()
        patch = catalog / "patch.toml"
        patch.write_text(patch_text(address, expected, "valid"))
        metadata = root / "param.json"
        metadata.write_text(json.dumps({"titleId": "TEST00001", "contentVersion": "01.000.000"}))
        digest = hashlib.sha256(image).hexdigest()
        def advise(count=1, *options):
            entry = {"titleId": "TEST00001", "contentVersion": "01.000.000", "patch": "patch.toml", "targets": {"eboot.bin": digest}}
            (catalog / "index.json").write_text(json.dumps({"format": 1, "entries": [entry] * count}))
            result = subprocess.run([sys.executable, str(advisor), "--catalog", str(catalog), "--param-json", str(metadata),
                                     "--input", str(source), "--relinker", str(relinker), "--skip-sce-module", "--json", *options],
                                    capture_output=True, text=True, timeout=10)
            data = json.loads(result.stdout)
            return result.returncode, data
        status, report = advise()
        assert status == 0 and report["status"] == "recommended", report
        record = report["entries"][0]
        assert record["status"] == "verified" and record["verification"]["summary"] == [1, 1, 0, 0], record
        assert record["targets"] == {"eboot.bin": digest}
        assert report["schemaVersion"] == 1, report
        assert record["conversion_options"] == ["--patches", str(patch), "--skip-sce-module"], record
        status, report = advise(2)
        assert status == 1 and report["status"] == "multiple_matches", report
        status, report = advise(2, "--list-verified")
        assert status == 0 and report["status"] == "verified_list" and len(report["entries"]) == 2, report
        patch.write_text(patch_text(address, expected ^ 1, "wrong_byte"))
        status, report = advise()
        assert status == 1 and report["status"] == "rejected", report
        assert report["entries"][0]["verification"]["exit_code"] == 1, report
        assert "conversion_options" not in report["entries"][0], report
        assert source.read_bytes() == image


if __name__ == "__main__":
    main()
