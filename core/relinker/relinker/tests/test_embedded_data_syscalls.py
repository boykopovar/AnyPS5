from pathlib import Path
import subprocess
import sys
import tempfile

from test_sce_code_extent import FUNCTION, FUNCTION_SIZE, LATER_FUNCTION, fixture
from test_sce_module_code_extent import run_module, sce_module


def tail(opcode, padding):
    routine = b"\x31\xc0\xb0\x3b" + opcode + b"\xc3" if opcode else b"\x31\xc0\x83\xc0\x01\xc3"
    return b"\x0f\x78\x11\x22\x33\x44" + routine + padding * 4 + b"\x31\xc0\xc3"


def run_executable(relinker, work, name, image, flags, expected_error):
    source = work / (name + ".elf")
    output = work / (name + ".out")
    source.write_bytes(image)
    result = subprocess.run([str(relinker), "--skip-sce-module", *flags, str(source), str(output)],
                            capture_output=True, text=True, timeout=20)
    if expected_error:
        assert result.returncode == 2 and expected_error in result.stderr and not output.exists(), (
            name, result.returncode, result.stdout, result.stderr)
    else:
        assert result.returncode == 0 and output.exists(), (name, result.stdout, result.stderr)


def main():
    relinker = Path(sys.argv[1]).resolve()
    group = sys.argv[2]
    assert group in ("executable", "guest"), group
    count = 0
    with tempfile.TemporaryDirectory(prefix="anyps5-embedded-syscalls-") as directory:
        work = Path(directory)
        for target, flags in (("linux", []), ("windows", ["--windows"]), ("macos", ["--macos"])):
            for name, opcode in (("syscall", b"\x0f\x05"), ("sysenter", b"\x0f\x34"),
                                 ("int80", b"\xcd\x80"), ("control", b"")):
                for padding in (b"\x00", b"\x90", b"\xcc"):
                    payload = tail(opcode, padding)
                    error = "Forbidden syscall instruction" if opcode else None
                    case = name + "-" + padding.hex() + "-" + target
                    if group == "guest":
                        image = sce_module(gap_embedded_data=True)
                        image[0x420:0x440] = payload + padding * (0x20 - len(payload))
                        run_module(relinker, work, case, image, flags, error)
                        if not opcode:
                            assert payload in (work / case / "app0" / "sce_module" / "libExample.prx.guest.prx").read_bytes(), case
                        count += 1
                    else:
                        for sce in (False, True):
                            image, _ = fixture(sce_block=sce, gap_embedded_data=True)
                            table = FUNCTION + FUNCTION_SIZE + 0x10
                            image[table:LATER_FUNCTION] = payload + padding * (LATER_FUNCTION - table - len(payload))
                            executable_case = ("sce-" if sce else "sysv-") + case
                            run_executable(relinker, work, executable_case, image, flags, error)
                            if not opcode:
                                assert payload in (work / (executable_case + ".out")).read_bytes(), executable_case
                            count += 1
    print("Embedded data syscall boundary tests passed:", group, count, "cases")


if __name__ == "__main__":
    main()
