from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_call_registry import make_image
from test_guest_intel_trampolines import main_fixture
from test_guest_none_relocations import TARGET, module


TARGETS = (("linux", []), ("pie", ["--lazy-binding"]), ("windows", ["--windows"]), ("macos", ["--macos"]))
ERROR = "Undefined non-weak symbol has non-default visibility: "


def executable(relocation, visibility, binding=1, referenced=True):
    image = make_image(b"\0lib.so\0symbol\0")
    image[0x63c] = (binding << 4) | 2
    image[0x63d] = visibility
    struct.pack_into("<QQq", image, 0x700, 0x300, (1 << 32) | (relocation if referenced else 0), 0)
    if relocation == 7:
        tags = [(1, 1), (5, 0x600), (10, 15), (6, 0x620), (11, 24), (0x6100003f, 48),
                (3, 0x300), (23, 0x700), (2, 24 if referenced else 0), (20, 7), (0, 0)]
        for index, tag in enumerate(tags):
            struct.pack_into("<qQ", image, 0x400 + index * 16, *tag)
        struct.pack_into("<QQ", image, 120 + 32, len(tags) * 16, len(tags) * 16)
    return image


def guest(relocation, visibility, binding=1, referenced=True, defined=False, sce=False):
    entry = (TARGET, (1 << 32) | (relocation if referenced else 0), 0)
    image = module(plt=[entry] if relocation == 7 else (),
                   rela=[entry] if relocation != 7 else (), exported=defined, sce=sce)
    image[0x229c] = (binding << 4) | 2
    image[0x229d] = visibility
    return image


def main():
    relinker = Path(sys.argv[1]).resolve()
    is_guest = sys.argv[2] == "guest"
    count = 0
    with tempfile.TemporaryDirectory(prefix="anyps5-restricted-symbols-") as directory:
        work = Path(directory)

        def convert(name, image, options, rejected):
            nonlocal count
            case = work / f"case-{count}"
            case.mkdir()
            source = case / "input.elf"
            source.write_bytes(main_fixture() if is_guest else image)
            if is_guest:
                modules = case / "sce_module"
                modules.mkdir()
                module_path = modules / "restricted.prx"
                module_path.write_bytes(image)
            output = case / "output.out"
            command = [str(relinker), *options, str(source), str(output)]
            if not is_guest:
                command.insert(1, "--skip-sce-module")
            result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", timeout=20)
            if rejected:
                symbol = "shared" if is_guest else "symbol"
                assert result.returncode == 2 and ERROR + symbol in result.stderr, (
                    name, options, result.returncode, result.stdout, result.stderr)
                assert not output.exists() and not (case / "app0").exists(), (name, options)
            else:
                assert result.returncode == 0 and output.is_file(), (
                    name, options, result.returncode, result.stdout, result.stderr)
            assert source.read_bytes() == (main_fixture() if is_guest else image), (name, options)
            if is_guest:
                assert module_path.read_bytes() == image, (name, options)
            count += 1

        for relocation in (1, 6, 7):
            for target, options in TARGETS:
                if is_guest and target == "pie":
                    continue
                for sce in ((False, True) if is_guest else (False,)):
                    make = (lambda visibility, **kw: guest(relocation, visibility, sce=sce, **kw)) if is_guest else (
                        lambda visibility, **kw: executable(relocation, visibility, **kw))
                    label = f"{relocation}-{target}-{sce}"
                    for visibility in (1, 2, 3):
                        convert(label + f"-visibility-{visibility}", make(visibility), options, True)
                    convert(label + "-local-hidden", make(2, binding=0), options, True)
                    convert(label + "-default", make(0), options, False)
                    convert(label + "-weak-default", make(0, binding=2), options, False)
                    convert(label + "-unreferenced", make(2, referenced=False), options, False)
                    if is_guest:
                        convert(label + "-defined-hidden", make(2, defined=True), options, False)
                    else:
                        convert(label + "-filtered", make(2), [*options, "unused-filter=1"], True)
    print(f"Restricted undefined symbol tests passed: {count} conversions ({'guest' if is_guest else 'executable'})")


if __name__ == "__main__":
    main()
