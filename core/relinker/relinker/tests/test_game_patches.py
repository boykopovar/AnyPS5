import os
from pathlib import Path
import subprocess
import sys
import tempfile

import struct

from test_guest_host_libc import DYNAMIC, importing
from test_guest_intel_trampolines import PLAIN_SITE, SITE, elf_loads, guest_fixture, main_fixture
from test_guest_syscall_trap import GENERIC_STUB
import test_jmprel_table_bounds
from test_guest_none_relocations import JUMP_SLOT, RELATIVE, TARGET, module


def patch_site(image):
    for _, flags, offset, mapped, _, file_size, _, _ in elf_loads(image):
        if flags & 1:
            return mapped + file_size - 1, image[offset + file_size - 1]
    raise AssertionError("No executable segment")


def patch_text(address, expected, name="", target="eboot.bin"):
    lines = ["format = 1", "", "[[patch]]"]
    if name:
        lines.append(f'name = "{name}"')
    lines += [f'target = "{target}"', f"va = {address:#x}", f'expected = "{expected:02x}"', f'value = "{expected ^ 1:02x}"', 'description = "flip one bit"', ""]
    return "\n".join(lines)


def main():
    relinker = Path(sys.argv[1]).resolve()
    image = bytes(main_fixture())
    address, expected = patch_site(image)
    with tempfile.TemporaryDirectory(prefix="anyps5-game-patches-") as directory:
        work = Path(directory)

        def run(*arguments):
            return subprocess.run([str(relinker), *arguments], capture_output=True, text=True, timeout=60)

        def case(name):
            root = work / name
            (root / "in").mkdir(parents=True)
            (root / "out").mkdir()
            (root / "in" / "eboot.bin").write_bytes(image)
            return root, root / "in" / "eboot.bin", root / "out" / "app.elf"

        root, source, output = case("plain")
        result = run("--skip-sce-module", str(source), str(output))
        assert result.returncode == 0, result.stderr
        baseline = output.read_bytes()
        assert not list(output.parent.glob("*.patches.toml")), "manifest written without patches"

        (source.parent / "relinker-patches.toml").write_text(patch_text(address, expected, "flip_bit"))
        result = run("--skip-sce-module", "--no-patches", str(source), str(output))
        assert result.returncode == 0 and output.read_bytes() == baseline, result.stderr
        assert "Patches:" not in result.stdout and not list(output.parent.glob("*.patches.toml"))

        result = run("--skip-sce-module", str(source), str(output))
        assert result.returncode == 0, (result.stdout, result.stderr)
        patched = output.read_bytes()
        assert patched != baseline
        assert f"Patches: {source.parent / 'relinker-patches.toml'}\n" in result.stdout, result.stdout
        assert f"Patch: eboot.bin [flip_bit] va={address:#x} off=" in result.stdout, result.stdout
        assert f"{expected:02x} -> {expected ^ 1:02x} (flip one bit)\n" in result.stdout, result.stdout
        manifest = output.parent / "app.patches.toml"
        text = manifest.read_text()
        assert 'main_target = "eboot.bin"' in text and 'name = "flip_bit"' in text, text
        assert 'main_target = "eboot.bin"\nlinux_base = 0x400000\n' in text, text
        assert f"Patch manifest: {manifest}\n" in result.stdout
        assert f'config = "{source.parent.as_posix()}/relinker-patches.toml"\n' in text, text

        if os.name != "nt":
            quoted = work / "quoted"
            (quoted / "out").mkdir(parents=True)
            (quoted / "eboot.bin").write_bytes(image)
            (quoted / 'we"ird.toml').write_text(patch_text(address, expected))
            result = run("--skip-sce-module", "--patches", str(quoted / 'we"ird.toml'), str(quoted / "eboot.bin"), str(quoted / "out" / "app.elf"))
            assert result.returncode == 2 and "Patch manifest cannot hold the config" in result.stderr, (result.stdout, result.stderr)
            assert not list((quoted / "out").iterdir()), "output written despite the manifest error"

            reduced = work / 'we"ird'
            (reduced / "out").mkdir(parents=True)
            (reduced / "eboot.bin").write_bytes(image)
            (reduced / "p.toml").write_text(patch_text(address, expected))
            result = run("--skip-sce-module", "--patches", str(reduced / "p.toml"), str(reduced / "eboot.bin"), str(reduced / "out" / "app.elf"))
            assert result.returncode == 0, (result.stdout, result.stderr)
            assert 'config = "p.toml"\n' in (reduced / "out" / "app.patches.toml").read_text()

        result = run("--verify-patches", str(manifest), str(source), "--skip-sce-module")
        assert result.returncode == 0 and "applicable: flip_bit eboot.bin" in result.stdout, (result.stdout, result.stderr)
        before = sorted(str(path) for path in work.rglob("*"))
        result = run("--verify-patches", str(manifest), str(output), "--verify-output")
        assert result.returncode == 0 and "applied: flip_bit eboot.bin" in result.stdout, (result.stdout, result.stderr)
        assert sorted(str(path) for path in work.rglob("*")) == before, "verify wrote files"
        renamed = work / "renamed"
        renamed.mkdir()
        (renamed / "eboot.bin").write_bytes(patched)
        result = run("--verify-patches", str(manifest), str(renamed / "eboot.bin"), "--skip-sce-module")
        assert result.returncode == 1 and "mismatch: flip_bit eboot.bin" in result.stdout, (result.stdout, result.stderr)

        other = work / "reproduce"
        other.mkdir()
        (source.parent / "relinker-patches.toml").unlink()
        result = run("--skip-sce-module", "--patches", str(manifest), str(source), str(other / "app.elf"))
        assert result.returncode == 0 and (other / "app.elf").read_bytes() == patched, result.stderr

        clash = work / "clash"
        (clash / "in").mkdir(parents=True)
        (clash / "in" / "eboot.bin").write_bytes(image)
        (clash / "in" / "app.patches.toml").write_text(patch_text(address, expected))
        result = run("--skip-sce-module", "--patches", str(clash / "in" / "app.patches.toml"), str(clash / "in" / "eboot.bin"), str(clash / "in" / "app.elf"))
        assert result.returncode == 2 and "would overwrite the patch file" in result.stderr, (result.stdout, result.stderr)
        assert not (clash / "in" / "app.elf").exists()

        hand = work / "hand.toml"
        hand.write_text(patch_text(address, expected))
        result = run("--verify-patches", str(hand), str(output), "--verify-output")
        assert result.returncode == 2 and "no main_target" in result.stderr, result.stderr

        root, source, output = case("wrong-bytes")
        (root / "p.toml").write_text(patch_text(address, expected ^ 0x55))
        result = run("--skip-sce-module", "--patches", str(root / "p.toml"), str(source), str(output))
        assert result.returncode == 2 and "expected" in result.stderr and "found" in result.stderr, result.stderr
        assert not list(output.parent.iterdir()), "output written despite a failed patch"
        result = run("--verify-patches", str(root / "p.toml"), str(source), "--skip-sce-module")
        assert result.returncode == 1 and "mismatch: eboot.bin" in result.stdout, result.stdout

        root, source, output = case("unmatched")
        (root / "p.toml").write_text(patch_text(address, expected, target="missing.prx"))
        result = run("--skip-sce-module", "--patches", str(root / "p.toml"), str(source), str(output))
        assert result.returncode == 2 and "unmatched patch target: missing.prx" in result.stderr, result.stderr
        assert not list(output.parent.iterdir())
        result = run("--verify-patches", str(root / "p.toml"), str(source), "--skip-sce-module")
        assert result.returncode == 2 and "--verify-output" in result.stderr, result.stderr

        root, source, output = case("extra")
        (root / "in" / "picker").mkdir()
        (root / "in" / "picker" / "picker.bin").write_bytes(image)
        (root / "p.toml").write_text(patch_text(address, expected, target="picker/picker.bin"))
        (root / "in" / "sce_module").mkdir()
        result = run("--executable", "picker/picker.bin", "--patches", str(root / "p.toml"), str(source), str(output))
        assert result.returncode == 0, (result.stdout, result.stderr)
        manifest = output.parent / "app.patches.toml"
        result = run("--verify-patches", str(manifest), str(output), "--verify-output", "--executable", "picker/picker.bin")
        assert result.returncode == 0 and "applied: picker/picker.bin" in result.stdout, (result.stdout, result.stderr)
        (output.parent / "picker.picker.bin.elf").unlink()
        result = run("--verify-patches", str(manifest), str(output), "--verify-output", "--executable", "picker/picker.bin")
        assert result.returncode == 2 and "output not found" in result.stderr, result.stderr

        relocation_overlap(relinker, work)
        writer_overlap(relinker, work)


def relocation_overlap(relinker, work):
    def run(*arguments):
        return subprocess.run([str(relinker), *arguments], capture_output=True, text=True, timeout=60)

    def patch_file(path, target, address, expected):
        path.write_text(f'format = 1\n\n[[patch]]\ntarget = "{target}"\nva = {address:#x}\nexpected = "{expected}"\nvalue = "ffff"\n')

    for kind, rela, plt in (("rela", [RELATIVE], []), ("plt", [], [JUMP_SLOT])):
        image = bytes(module(rela, plt))
        for offset, overlaps in ((TARGET + 4, True), (TARGET + 8, False)):
            case = work / f"mod-{kind}-{offset:x}"
            (case / "in" / "sce_module").mkdir(parents=True)
            (case / "in" / "sce_module" / "module.prx").write_bytes(image)
            (case / "in" / "eboot.bin").write_bytes(main_fixture())
            (case / "out").mkdir()
            expected = image[0x2000 + (offset - 0x2000):0x2000 + (offset - 0x2000) + 2].hex()
            patch_file(case / "p.toml", "sce_module/module.prx", offset, expected)
            result = run("--patches", str(case / "p.toml"), str(case / "in" / "eboot.bin"), str(case / "out" / "app.elf"))
            if overlaps:
                assert result.returncode == 2 and "relocation target" in result.stderr, (kind, result.stdout, result.stderr)
                assert not list((case / "out").iterdir()), "output written despite the overlap"
            else:
                assert result.returncode == 0, (kind, result.stdout, result.stderr)

    image = bytearray(importing())
    struct.pack_into("<qQ", image, DYNAMIC + 5 * 16, 8, 24)
    address = 0x4d00
    struct.pack_into("<QQq", image, 0x4700, address, 8, 0x4000)
    for offset, overlaps in ((address + 2, True), (address + 8, False)):
        case = work / f"exe-{offset:x}"
        (case / "out").mkdir(parents=True)
        (case / "eboot.bin").write_bytes(image)
        patch_file(case / "p.toml", "eboot.bin", offset, bytes(image[offset:offset + 2]).hex())
        result = run("--skip-sce-module", "--patches", str(case / "p.toml"), str(case / "eboot.bin"), str(case / "out" / "app.elf"))
        if overlaps:
            assert result.returncode == 2 and "RelaData relocation target" in result.stderr, (result.stdout, result.stderr)
            assert not list((case / "out").iterdir())
        else:
            assert result.returncode == 0, (result.stdout, result.stderr)



def writer_overlap(relinker, work):
    def run(*arguments):
        return subprocess.run([str(relinker), *arguments], capture_output=True, text=True, timeout=60)

    def attempt(name, module, options, address, expected, value="ff", target="sce_module/sample.prx"):
        case = work / name
        (case / "in" / "sce_module").mkdir(parents=True)
        (case / "in" / "sce_module" / "sample.prx").write_bytes(module)
        (case / "in" / "eboot.bin").write_bytes(main_fixture())
        (case / "out").mkdir()
        (case / "p.toml").write_text(f'format = 1\n\n[[patch]]\ntarget = "{target}"\nva = {address:#x}\nexpected = "{expected}"\nvalue = "{value}"\n')
        result = run(*options, "--patches", str(case / "p.toml"), str(case / "in" / "eboot.bin"), str(case / "out" / "app.elf"))
        return result, case / "out"

    intel = bytes(guest_fixture(SITE))
    result, out = attempt("intel-hit", intel, ["--to-intel"], 0x1007, "08", "09")
    assert result.returncode == 2 and "overlaps Intel" in result.stderr, (result.stdout, result.stderr)
    assert not list(out.iterdir())
    result, out = attempt("intel-clear", intel, ["--to-intel"], 0x1020, "90")
    assert result.returncode == 0, (result.stdout, result.stderr)

    trap = bytearray(guest_fixture(PLAIN_SITE))
    trap[0x400:0x400 + len(GENERIC_STUB)] = GENERIC_STUB
    trap = bytes(trap)
    result, out = attempt("trap-hit", trap, ["--trap-sdk-syscall-stubs"], 0x1017, "0f05", "0f05")
    assert result.returncode == 2 and "overlaps syscall trap" in result.stderr, (result.stdout, result.stderr)
    assert not list(out.iterdir())
    result, out = attempt("trap-clear", trap, ["--trap-sdk-syscall-stubs"], 0x1000, "48", "48")
    assert result.returncode == 0, (result.stdout, result.stderr)

    image = bytes(test_jmprel_table_bounds.fixture(linux=True))
    for offset, overlaps in ((0x2ff, True), (0x2fe, False), (0x307, True), (0x308, False)):
        case = work / f"exe-plt-{offset:x}"
        (case / "out").mkdir(parents=True)
        (case / "eboot.bin").write_bytes(image)
        (case / "p.toml").write_text(f'format = 1\n\n[[patch]]\ntarget = "eboot.bin"\nva = {offset:#x}\nexpected = "{image[offset:offset + 2].hex()}"\nvalue = "ffff"\n')
        result = run("--skip-sce-module", "--patches", str(case / "p.toml"), str(case / "eboot.bin"), str(case / "out" / "app.elf"))
        if overlaps:
            assert result.returncode == 2 and "RelaPltData relocation target" in result.stderr, (offset, result.stdout, result.stderr)
            assert not list((case / "out").iterdir())
        else:
            assert result.returncode == 0, (offset, result.stdout, result.stderr)


if __name__ == "__main__":
    main()
