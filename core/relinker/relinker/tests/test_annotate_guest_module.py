import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))
import annotate_module

from test_guest_intel_trampolines import main_fixture
from test_guest_none_relocations import JUMP_SLOT, module


def convert(root, rela, plt):
    (root / "in" / "sce_module").mkdir(parents=True)
    (root / "in" / "sce_module" / "module.prx").write_bytes(bytes(module(rela, plt)))
    (root / "in" / "eboot.bin").write_bytes(bytes(main_fixture()))
    (root / "out").mkdir()
    result = subprocess.run([sys.argv[1], str(root / "in" / "eboot.bin"), str(root / "out" / "app.elf")],
                            capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stderr
    artifacts = sorted((root / "out").rglob("*.guest.prx"))
    assert len(artifacts) == 1, artifacts
    return artifacts[0]


def parsed(path):
    return annotate_module.Module(path.read_bytes())


def main():
    with tempfile.TemporaryDirectory(prefix="anyps5-annotate-guest-") as directory:
        root = Path(directory)

        imported = parsed(convert(root / "imports", [], [JUMP_SLOT]))
        assert imported.symbol_count >= 2, imported.symbol_count
        slots = imported.imports()
        assert len(slots) == 1, slots
        assert "shared" in list(slots.values())[0], slots
        exports, thread_local = imported.exports()
        assert len(exports) == 1 and not thread_local, (exports, thread_local)
        assert list(exports.values())[0] == ["shared#guest"], exports
        assert imported.code_segments(), "no code segments"

        plain = parsed(convert(root / "plain", [], []))
        assert plain.imports() == {}, plain.imports()
        exports, thread_local = plain.exports()
        assert len(exports) == 1 and not thread_local, (exports, thread_local)
        assert plain.code_segments(), "no code segments"
    return 0


if __name__ == "__main__":
    sys.exit(main())
