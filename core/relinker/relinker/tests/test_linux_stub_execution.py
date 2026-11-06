"""Run a relinked Linux executable whose AMD-only instruction was replaced by a stub."""

from pathlib import Path
import platform
import struct
import subprocess
import sys
import tempfile

from test_linux_load_alignment import fixture

LINUX_BASE = 0x400000
CODE_VADDR = 0x10
DATA_VADDR = 0x100
CODE_OFFSET = 0x4000 + CODE_VADDR
DATA_OFFSET = 0x4000 + DATA_VADDR
EXPECTED = 42
ADDEND = 2


def payload(relocated, branch=False, shani=False, counted=False, incoming=False):
    """Produce an exit code through an AMD-only instruction, optionally followed by a site that
    the converter has to move into the stub and relocate."""
    dead = b"\xbf\x63\x00\x00\x00"          # mov edi, 99: only runs if a relocation is wrong
    code = b""

    def rip(opcode, target):
        return opcode + struct.pack("<i", target - (CODE_VADDR + len(code) + len(opcode) + 4))

    if shani:
        code += b"\x66\x0f\xef\xc0"          # pxor xmm0, xmm0
        code += rip(b"\x0f\x38\xc8\x05", DATA_VADDR + 48)
        code += b"\x66\x48\x0f\x7e\xc7"      # movq rdi, xmm0
        code += b"\xb8\xe7\x00\x00\x00"      # mov  eax, 231
        code += b"\x0f\x05"                  # syscall
        return code

    if counted:
        code += b"\x31\xc9"                  # xor ecx, ecx
    code += rip(b"\x48\x8d\x35", DATA_VADDR)  # lea rsi, [rip+data]
    code += b"\xf3\x0f\x6f\x06"              # movdqu xmm0, [rsi]
    code += b"\xf3\x0f\x6f\x4e\x10"          # movdqu xmm1, [rsi+16]
    code += b"\x66\x0f\x79\xc1"              # extrq  xmm0, xmm1
    if relocated:
        code += rip(b"\x66\x0f\xfe\x05", DATA_VADDR + 32)
    if branch:
        code += b"\xeb" + bytes([len(dead)]) + dead
    if counted:
        code += b"\xe3" + bytes([len(dead)]) + dead
    if incoming:
        code += b"\xf3\x0f\x6f\x56\x20"      # movdqu xmm2, [rsi+32]  (the addend)
        skipped = b"\xbf\x63\x00\x00\x00"    # mov edi, 99
        site = b"\x66\x0f\x79\xd9"           # extrq xmm3, xmm1: a four byte site, harmless
        absorbed = b"\x66\x0f\xfe\xc2"       # paddd xmm0, xmm2: absorbed, and the jump target
        target = CODE_VADDR + len(code) + 5 + len(skipped) + len(site)
        code += b"\xe9" + struct.pack("<i", target - (CODE_VADDR + len(code) + 5))
        code += skipped + site + absorbed
    code += b"\x66\x48\x0f\x7e\xc7"          # movq rdi, xmm0
    code += b"\xb8\xe7\x00\x00\x00"          # mov  eax, 231
    code += b"\x0f\x05"                      # syscall
    return code


def source(relocated=False, branch=False, shani=False, counted=False, incoming=False):
    image = fixture()
    struct.pack_into("<Q", image, 24, CODE_VADDR)
    code = payload(relocated, branch, shani, counted, incoming)
    image[CODE_OFFSET:CODE_OFFSET + len(code)] = code
    # The field sits in bits 15:8, so a length of 8 and an index of 8 extract it.
    extracted = EXPECTED - ADDEND if (relocated or incoming) else EXPECTED
    struct.pack_into("<QQ", image, DATA_OFFSET, extracted << 8, 0)
    struct.pack_into("<QQ", image, DATA_OFFSET + 16, 8 | (8 << 8), 0)
    struct.pack_into("<QQ", image, DATA_OFFSET + 32, ADDEND, 0)
    struct.pack_into("<QQ", image, DATA_OFFSET + 48, EXPECTED, 0)
    return image


def segments(elf):
    phoff, = struct.unpack_from("<Q", elf, 32)
    phentsize, phnum = struct.unpack_from("<HH", elf, 54)
    return [struct.unpack_from("<IIQQQQQQ", elf, phoff + index * phentsize) for index in range(phnum)]


def read_vaddr(elf, vaddr, size):
    for kind, _, offset, segment, _, filesz, _, _ in segments(elf):
        if kind == 1 and segment <= vaddr < segment + filesz:
            start = offset + vaddr - segment
            return elf[start:start + size]
    raise AssertionError(f"virtual address {vaddr:#x} is not mapped")


def relink(relinker, work, name, relocated, branch=False, shani=False, expect="EXTRQ register form", counted=False, incoming=False):
    elf = work / (name + ".elf")
    output = work / (name + ".out")
    elf.write_bytes(source(relocated, branch, shani, counted, incoming))
    result = subprocess.run([str(relinker), "--skip-sce-module", "--skip-syscall-check", "--to-intel", "--linux-fixed-base", hex(LINUX_BASE),
                             str(elf), str(output)], capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, (name, result.stdout, result.stderr)
    assert expect in result.stdout, result.stdout
    return output


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-stub-exec-") as directory:
        work = Path(directory)
        elf = work / "input.elf"
        output = work / "output.elf"
        elf.write_bytes(source())
        result = subprocess.run([str(relinker), "--skip-sce-module", "--skip-syscall-check", "--to-intel", "--linux-fixed-base", hex(LINUX_BASE),
                                 str(elf), str(output)], capture_output=True, text=True, timeout=60)
        assert result.returncode == 0, (result.stdout, result.stderr)
        assert "EXTRQ register form" in result.stdout, result.stdout

        relinked = output.read_bytes()
        site = read_vaddr(relinked, LINUX_BASE + CODE_VADDR + 16, 5)
        assert site[0] == 0xE9, "the AMD-only site was not replaced by a jump"
        target = LINUX_BASE + CODE_VADDR + 21 + struct.unpack_from("<i", site, 1)[0]
        stub = read_vaddr(relinked, target, 0x200)
        assert stub[0] == 0x48, "the stub does not start with the red-zone skip"
        executable = [header for header in segments(relinked)
                      if header[0] == 1 and header[1] & 1 and header[3] <= target < header[3] + header[6]]
        assert executable, "the stub is not inside an executable segment"

        if platform.system() == "Linux" and platform.machine() == "x86_64":
            output.chmod(0o755)
            run = subprocess.run([str(output)], capture_output=True, timeout=60)
            assert run.returncode == EXPECTED, f"the relinked executable returned {run.returncode}"
            relocated = relink(relinker, work, "relocated", True)
            relocated.chmod(0o755)
            run = subprocess.run([str(relocated)], capture_output=True, timeout=60)
            assert run.returncode == EXPECTED, f"the relocated RIP-relative operand returned {run.returncode}"
            branched = relink(relinker, work, "branched", False, True)
            branched.chmod(0o755)
            run = subprocess.run([str(branched)], capture_output=True, timeout=60)
            assert run.returncode == EXPECTED, f"the relocated branch returned {run.returncode}"
            shani = relink(relinker, work, "shani", False, False, True, "SHA1NEXTE")
            shani.chmod(0o755)
            run = subprocess.run([str(shani)], capture_output=True, timeout=60)
            assert run.returncode == EXPECTED, f"the RIP-relative SHA-NI operand returned {run.returncode}"
            counted = relink(relinker, work, "counted", False, False, False, "EXTRQ register form", True)
            counted.chmod(0o755)
            run = subprocess.run([str(counted)], capture_output=True, timeout=60)
            assert run.returncode == EXPECTED, f"the relocated JRCXZ returned {run.returncode}"
            incoming = relink(relinker, work, "incoming", False, False, False, "EXTRQ register form", False, True)
            incoming.chmod(0o755)
            run = subprocess.run([str(incoming)], capture_output=True, timeout=60)
            assert run.returncode == EXPECTED, f"the branch redirected into the stub returned {run.returncode}"
            print("Linux stub execution tests passed (five scenarios executed)")
            return
        relink(relinker, work, "relocated", True)
        relink(relinker, work, "branched", False, True)
        relink(relinker, work, "shani", False, False, True, "SHA1NEXTE")
        relink(relinker, work, "counted", False, False, False, "EXTRQ register form", True)
        relink(relinker, work, "incoming", False, False, False, "EXTRQ register form", False, True)
    print("Linux stub execution tests passed (inspection only)")


if __name__ == "__main__":
    main()
