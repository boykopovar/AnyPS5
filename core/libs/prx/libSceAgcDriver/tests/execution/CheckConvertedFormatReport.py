import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


executable = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory() as directory:
    copy = Path(shutil.copy2(executable, directory))
    result = subprocess.run([str(copy), "--unsigned-packed32", "37"])
    if result.returncode:
        sys.exit(result.returncode)
    report = json.loads((copy.parent / "anyps5-report.json").read_text(encoding="utf-8"))

formats = report["shaders"]["imageFormats"]
if not any(row["format"] == 37 and row["status"] == "accepted" and row["count"] > 0 for row in formats):
    sys.exit(f"converted texture format 37 missing from accepted lookups: {formats}")
