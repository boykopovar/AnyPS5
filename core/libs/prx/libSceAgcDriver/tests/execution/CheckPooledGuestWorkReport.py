from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


executable = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory() as directory:
    copy = Path(shutil.copy2(executable, directory))
    sys.exit(subprocess.run([str(copy), "--pooled", "--guest-work"]).returncode)
