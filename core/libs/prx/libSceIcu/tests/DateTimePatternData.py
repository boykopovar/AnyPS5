import argparse
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("source", type=Path)
parser.add_argument("build", type=Path)
parser.add_argument("icupkg", type=Path)
args = parser.parse_args()
data = args.build / "icu-data"
manifest = set((data / "tmp/icudata.lst").read_text().splitlines())
packaged = set(subprocess.check_output([str(args.icupkg), "-l", str(data / "icudt78l.dat")], text=True).splitlines())
assert manifest == packaged, "generated manifest and embedded package inputs differ"
for source, prefix in (("locales", ""), ("curr", "curr/"), ("zone", "zone/"), ("misc", "")):
    expected = {prefix + path.stem + ".res" for path in (args.source / "data" / source).glob("*.txt")}
    assert expected, source
    missing = expected - packaged
    assert not missing, (source, sorted(missing))
    if source != "misc":
        assert prefix + "res_index.res" in packaged, source
    print(source, len(expected), "complete source resources")
normalization = {path.name for path in (args.source / "data/in").glob("*.nrm")} - {"nfc.nrm"}
assert normalization <= packaged, sorted(normalization - packaged)
for name in ("brkitr/root.res", "uemoji.icu", "ulayout.icu"):
    assert name in packaged, name
print(len(packaged), "generated package entries match the manifest")
