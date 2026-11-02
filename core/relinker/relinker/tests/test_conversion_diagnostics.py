from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_optional_plt import fixture
from test_guest_intel_trampolines import guest_fixture, main_fixture, PLAIN_SITE


def main():
    relinker = Path(sys.argv[1]).resolve()
    failures = []
    with tempfile.TemporaryDirectory(prefix="anyps5-diagnostics-") as directory:
        work = Path(directory) / "title space \u00e9"
        work.mkdir()

        def convert(name, image, expected, flags, module=None, extra=False, module_name="broken \u00e9.prx"):
            case = work / name
            case.mkdir()
            source = case / "eboot.bin"
            source.write_bytes(main_fixture() if module is not None or extra else image)
            options = []
            offending = source
            if module is not None:
                modules = case / "sce_module"
                modules.mkdir()
                offending = modules / module_name
                offending.write_bytes(module)
                options = ["--sce-module-path", str(case)]
            elif extra:
                offending = case / "picker.bin"
                offending.write_bytes(image)
                (case / "sce_module").mkdir()
                options = ["--sce-module-path", str(case), "--executable", "picker.bin"]
            else:
                options = ["--skip-sce-module"]
            output = case / "converted.out"
            result = subprocess.run([str(relinker), *flags, *options, str(source), str(output)],
                                    capture_output=True, text=True, encoding="utf-8", timeout=20)
            valid = (result.returncode == 2 and expected in result.stderr
                     and result.stderr.splitlines().count(f"Input: {offending}") == 1
                     and not output.exists() and not list(case.rglob("*.guest.prx")))
            if extra:
                valid = valid and not (case / "picker.bin.out").exists()
            if not valid:
                failures.append((name, result.returncode, result.stdout, result.stderr))

        cases = []
        for tag, value, message in (
            (11, 16, "Unsupported DT_SYMENT value: 16; expected 24 bytes"),
            (9, 8, "Unsupported DT_RELAENT value: 8; expected 24 bytes"),
            (8, 25, "Invalid DT_RELASZ value: 25 bytes; expected a multiple of 24 bytes"),
        ):
            image = fixture()
            for offset in range(0x400, 0x470, 16):
                if struct.unpack_from("<q", image, offset)[0] == tag:
                    struct.pack_into("<Q", image, offset + 8, value)
            cases.append((str(tag), image, message))
        missing = fixture()
        struct.pack_into("<q", missing, 0x400, 0x70000000)
        cases.append(("missing", missing, "Neither DT_OS_ nor DT_ variant present for DT_STRTAB (tags 0x61000035 and 0x5)"))
        cases.append(("plt-type", fixture([(3, 0x300), (2, 0), (20, 17), (23, 0x720)]),
                      "Unsupported DT_PLTREL type: 17; expected DT_RELA (7)"))
        cases.append(("plt-size", fixture([(3, 0x300), (2, 25), (20, 7), (23, 0x720)]),
                      "Invalid DT_PLTRELSZ value: 25 bytes; expected a multiple of 24 bytes"))
        invalid_header = fixture()
        struct.pack_into("<I", invalid_header, 20, 2)
        cases.append(("header", invalid_header, "Unsupported ELF object version: expected 1 (offset 0x14)"))
        for index, flags in enumerate(([], ["--windows"], ["--to-intel"], ["--windows", "--to-intel"])):
            for name, image, message in cases:
                convert(f"{index}-{name}", image, message, flags)
            convert(f"{index}-extra", cases[0][1], cases[0][2], flags, extra=True)
            if "--windows" in flags:
                convert(f"{index}-module-name", None, "Unsupported Windows guest filename", flags, module=guest_fixture(PLAIN_SITE))
            module = guest_fixture(PLAIN_SITE)
            struct.pack_into("<QQ", module, 176 + 32, 17, 17)
            convert(f"{index}-module", None, "Unterminated dynamic segment", flags, module=module)
            convert(f"{index}-module-syscall", None, "Forbidden syscall instruction", flags, module=guest_fixture(b"\x0f\x05" + b"\x90" * 4), module_name="broken.prx")
    if failures:
        raise AssertionError(failures)
    print("Conversion diagnostics tests passed")


if __name__ == "__main__":
    main()
