from pathlib import Path
import platform
import struct
import subprocess
import sys

from guest_import_fixtures import fixture_directory
from test_guest_intel_trampolines import elf_loads, main_fixture
from test_guest_module_directories import module_with_symbol
from test_guest_tls import static_tls_getter
from test_macos_output import MachO
from test_macos_executable_exports import executable as exported_executable, exports
from test_macos_tls_exports import guest as tls_export
from test_tls_function_coverage import TLS_LOAD, add_alias, make_image, pe_bytes_at


RUNNER = Path(__file__).with_name("windows_image_runner.py")
RUNS_ON_HOST = sys.platform == "linux" and platform.machine() == "x86_64"
TARGETS = ("linux", "windows", "macos")


def module_image(size, second=False):
    original = module_with_symbol(True)
    original[0x400:0x406] = bytes.fromhex("b8 2a 00 00 00 c3")
    struct.pack_into("<Q", original, 0x8a8, size)
    if second:
        strings = b"\0shared#A#B\0other#A#B\0"
        original[0x800:0x800 + len(strings)] = strings
        struct.pack_into("<Q", original, 0x618, len(strings))
        struct.pack_into("<IIIIII", original, 0x840, 1, 3, 1, 0, 0, 0)
        struct.pack_into("<IBBHQQ", original, 0x8b0, 12, 0x12, 0, 1, 0x1010, 6)
        original[0x410:0x416] = bytes.fromhex("b8 2b 00 00 00 c3")
    image = bytearray(0x3000)
    image[:0x400] = original[:0x400]
    for index in range(3):
        header = list(struct.unpack_from("<IIQQQQQQ", original, 64 + index * 56))
        image[header[3]:header[3] + header[5]] = original[header[2]:header[2] + header[5]]
        header[2] = header[3]
        if header[0] == 1:
            header[7] = 0x1000
        struct.pack_into("<IIQQQQQQ", image, 64 + index * 56, *header)
    return image


def elf_symbol_sizes(data):
    loads = elf_loads(data)

    def offset(address):
        for header in loads:
            if header[3] <= address < header[3] + header[5]:
                return header[2] + address - header[3]
        raise AssertionError(f"Unmapped ELF address {address:#x}")

    phoff = struct.unpack_from("<Q", data, 32)[0]
    phsize, phcount = struct.unpack_from("<HH", data, 54)
    dynamic = next(struct.unpack_from("<IIQQQQQQ", data, phoff + index * phsize)
                   for index in range(phcount)
                   if struct.unpack_from("<I", data, phoff + index * phsize)[0] == 2)
    tags = {}
    for position in range(dynamic[2], dynamic[2] + dynamic[5], 16):
        tag, value = struct.unpack_from("<qQ", data, position)
        if tag == 0:
            break
        tags[tag] = value
    count = struct.unpack_from("<I", data, offset(tags[4]) + 4)[0]
    strings = offset(tags[5])
    symbols = offset(tags[6])
    sizes = {}
    for index in range(count):
        name, _, _, _, _, size = struct.unpack_from("<IBBHQQ", data, symbols + index * 24)
        if name:
            text = data[strings + name:data.index(0, strings + name)].decode()
            sizes[text] = size
    return sizes


def convert(relinker, work, name, target, image, module=False, roots=True):
    case = work / f"{name}-{target}-{'roots' if roots else 'strict'}"
    case.mkdir()
    source = case / "input.elf"
    source.write_bytes(main_fixture() if module else image)
    provider = None
    if module:
        directory = case / "sce_module"
        directory.mkdir()
        provider = directory / "provider.prx"
        provider.write_bytes(image)
    output = case / ("output.exe" if target == "windows" else "output")
    options = [] if target == "linux" else [f"--{target}"]
    if not module:
        options.append("--skip-sce-module")
    if roots:
        options.append("--function-symbol-roots")
    run = subprocess.run([str(relinker), *options, str(source), str(output)],
                         capture_output=True, text=True, timeout=10)
    assert source.read_bytes() == (main_fixture() if module else image)
    if module:
        assert provider.read_bytes() == image
        artifact = case / "app0/sce_module/provider.prx.guest.prx"
    else:
        artifact = output
    return run, artifact


def succeeded(run, artifact, target):
    assert run.returncode == 0, (artifact, run.returncode, run.stdout, run.stderr)
    data = artifact.read_bytes()
    magic = {"linux": b"\x7fELF", "windows": b"MZ", "macos": bytes.fromhex("cf fa ed fe")}
    assert data.startswith(magic[target]), artifact
    return data


def execute_windows(artifact):
    if sys.platform == "win32":
        command = [str(artifact)]
    elif RUNS_ON_HOST:
        command = [sys.executable, str(RUNNER), str(artifact), "0x11200"]
    else:
        return
    called = subprocess.run(command, capture_output=True, text=True, timeout=10)
    assert called.returncode == 42, (artifact, called.returncode, called.stdout, called.stderr)


def executable_cases(relinker, work, baseline):
    cases = (("overrun", make_image("register", "symbol", extent=0x1000)),
             ("boundary", make_image("register", "symbol", extent=0x46)),
             ("overlap", add_alias(make_image("register", "symbol", extent=0x46), 0x10, 0x1240)))
    for name, image in cases:
        for target in TARGETS:
            if target != "linux":
                strict, artifact = convert(relinker, work, name, target, image, roots=False)
                assert strict.returncode == 2 and "Code analysis:" in strict.stderr, (name, target, strict.stderr)
                assert not artifact.exists(), artifact
            if baseline:
                continue
            run, artifact = convert(relinker, work, name, target, image)
            data = succeeded(run, artifact, target)
            if target == "windows":
                assert pe_bytes_at(data, 0x11240, 1) == b"\xe9"
                assert pe_bytes_at(data, 0x11850, len(TLS_LOAD)) == TLS_LOAD
                execute_windows(artifact)
            elif target == "macos":
                code = b"".join(data[begin:end] for begin, end in MachO(data).executable_ranges())
                assert code.count(TLS_LOAD) == 1, (name, code.count(TLS_LOAD))
    image = exported_executable((("FixtureExport#A#B", 0x12, 0, 1, 0x210, 0x1000),))
    strict, artifact = convert(relinker, work, "export", "macos", image, roots=False)
    assert strict.returncode == 2, (strict.returncode, strict.stderr)
    assert not artifact.exists(), artifact
    if not baseline:
        run, artifact = convert(relinker, work, "export", "macos", image)
        data = succeeded(run, artifact, "macos")
        base = MachO(data).segment("__ELF0")["vmaddr"]
        assert exports(data)["_FixtureExport"] == (0, base + 0x210 - 0x140000000)
        image = make_image("register", "symbol", extent=0x1000,
                           body=b"\xff\x25" + struct.pack("<i", 0x780 - 0x1246))
        struct.pack_into("<Q", image, 0x780, 0x3000)
        struct.pack_into("<QQq", image, 0x700, 0x780, 8, 0x3000)
        position = (len(image) + 0xfff) & ~0xfff
        image.extend(b"\x90" * (position + 0x1000 - len(image)))
        struct.pack_into("<H", image, 56, 5)
        struct.pack_into("<IIQQQQQQ", image, 288, 1, 5, position, 0x3000, 0x3000, 0x1000, 0x1000, 0x1000)
        code = TLS_LOAD + bytes.fromhex("8b 40 f0 c3")
        image[position:position + len(code)] = code
        for target in ("windows", "macos"):
            run, artifact = convert(relinker, work, "cross-segment", target, image)
            data = succeeded(run, artifact, target)
            if target == "windows":
                assert pe_bytes_at(data, 0x13000, 1) == b"\xe9"
                if sys.platform == "win32":
                    execute_windows(artifact)
            else:
                code = b"".join(data[begin:end] for begin, end in MachO(data).executable_ranges())
                assert code.count(TLS_LOAD) == 1


def module_cases(relinker, work, baseline):
    for name, size, second in (("overrun", 0x1000, False), ("boundary", 2, False),
                               ("overlap", 0x12, True)):
        image = module_image(size, second)
        for target in TARGETS:
            if name == "overrun":
                strict, artifact = convert(relinker, work, name, target, image, module=True, roots=False)
                assert strict.returncode == 2, (name, target, strict.returncode, strict.stderr)
                assert not artifact.exists(), artifact
            if baseline:
                continue
            run, artifact = convert(relinker, work, name, target, image, module=True)
            data = succeeded(run, artifact, target)
            if target == "linux":
                sizes = elf_symbol_sizes(data)
                assert sizes["shared#guest"] == size, sizes
                if second:
                    assert sizes["other#guest"] == 6, sizes
                if RUNS_ON_HOST:
                    call = "import ctypes, sys; lib=ctypes.CDLL(sys.argv[1]); sys.exit(getattr(lib, sys.argv[2])())"
                    for symbol, expected in (("shared#guest", 42),) + ((("other#guest", 43),) if second else ()):
                        called = subprocess.run([sys.executable, "-c", call, str(artifact), symbol],
                                                capture_output=True, text=True, timeout=10)
                        assert called.returncode == expected, (name, symbol, called.returncode, called.stderr)
    for name, extent in (("tls-overrun", 0x1000), ("tls-boundary", 6)):
        image = static_tls_getter(16, 16)
        code = TLS_LOAD + bytes.fromhex("8b 40 f0 c3")
        image[0x400:0x400 + len(code)] = code
        struct.pack_into("<Q", image, 0x8c0, extent)
        for target in TARGETS:
            if name == "tls-overrun" or target != "linux":
                strict, artifact = convert(relinker, work, name, target, image, module=True, roots=False)
                assert strict.returncode == 2, (name, target, strict.returncode, strict.stderr)
                assert not artifact.exists(), artifact
            if baseline:
                continue
            run, artifact = convert(relinker, work, name, target, image, module=True)
            data = succeeded(run, artifact, target)
            if target == "windows":
                assert pe_bytes_at(data, 0x10000, 1) == b"\xe9"
            elif target == "macos":
                code = b"".join(data[begin:end] for begin, end in MachO(data).executable_ranges())
                assert TLS_LOAD not in code


def rejected_cases(relinker, work):
    cases = []
    for name, value, info, size in (("unmapped-function", 0x3000, 0x12, 1),
                                    ("data-function", 0x2000, 0x12, 1),
                                    ("oversized-data", 0x2000, 0x11, 0x1000)):
        image = module_image(6)
        struct.pack_into("<IBBHQQ", image, 0x2280 + 24, 1, info, 0, 1, value, size)
        cases.append((name, image))
    cases.append(("oversized-tls", tls_export(0x40, 1)))
    image = module_image(6)
    struct.pack_into("<Q", image, 64 + 40, 0x200)
    struct.pack_into("<Q", image, 0x2298 + 8, 0x1100)
    cases.append(("bss-function", image))
    for name, image in cases:
        for target in TARGETS:
            run, artifact = convert(relinker, work, name, target, image, module=True)
            assert run.returncode == 2, (name, target, run.returncode, run.stdout, run.stderr)
            assert not artifact.exists(), artifact
    image = make_image("register", "symbol")
    struct.pack_into("<Q", image, 24, 0x2000)
    for target in ("windows", "macos"):
        run, artifact = convert(relinker, work, "invalid-entry", target, image)
        assert run.returncode == 2, (target, run.returncode, run.stderr)
        assert not artifact.exists(), artifact
    truncated = make_image("register", "symbol", extent=0x1000,
                           body=b"\xe9" + struct.pack("<i", 0x1fff - 0x1245))
    truncated[0x1fff] = 0xb8
    fallthrough = make_image("register", "symbol", extent=0x1000,
                            body=b"\xe9" + struct.pack("<i", 0x1fff - 0x1245))
    fallthrough[0x1fff] = 0x90
    outside = make_image("register", "symbol", extent=0x1000,
                         body=b"\xe9" + struct.pack("<i", 0x3000 - 0x1245))
    zero_tail = make_image("register", "symbol", extent=0x1000,
                           body=b"\xe9" + struct.pack("<i", 0x1ffc - 0x1245))
    zero_tail[0x1ffc:0x2000] = bytes.fromhex("90 00 00 00")
    call_tail = make_image("register", "symbol", extent=0x1000,
                           body=b"\xe9" + struct.pack("<i", 0x1ff8 - 0x1245))
    call_tail[0x1250] = 0xc3
    call_tail[0x1ff8:0x2000] = b"\xe8" + struct.pack("<i", 0x1250 - 0x1ffd) + bytes(3)
    indirect = make_image("register", "symbol", extent=0x1000,
                          body=b"\xff\x25" + struct.pack("<i", 0x780 - 0x1246))
    struct.pack_into("<Q", indirect, 0x780, 0x800)
    struct.pack_into("<QQq", indirect, 0x700, 0x780, 8, 0x800)
    for name, image in (("oversized-fde", make_image("register", "unwind", extent=0x1000)),
                        ("truncated", truncated), ("fallthrough", fallthrough), ("outside", outside),
                        ("zero-tail", zero_tail), ("call-tail", call_tail), ("indirect", indirect)):
        for target in ("windows", "macos"):
            run, artifact = convert(relinker, work, name, target, image)
            diagnostic = "Instruction truncated" if name in ("truncated", "zero-tail", "call-tail") else "Code analysis:"
            assert run.returncode == 2 and diagnostic in run.stderr, (name, target, run.returncode, run.stderr)
            assert not artifact.exists(), artifact


def main():
    relinker = Path(sys.argv[1]).resolve()
    group = sys.argv[2]
    baseline = len(sys.argv) > 3 and sys.argv[3] == "baseline"
    with fixture_directory("anyps5-function-symbol-roots-") as directory:
        work = Path(directory)
        if group == "executable":
            executable_cases(relinker, work, baseline)
        elif group == "module":
            module_cases(relinker, work, baseline)
        elif group == "rejected":
            rejected_cases(relinker, work)
        else:
            raise ValueError(group)
    print(f"Function-symbol roots {group} tests passed")


if __name__ == "__main__":
    main()
