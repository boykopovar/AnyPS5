"""Check the Mach-O that `relinker --macos` writes from small byte-built guests.

The output only runs on macOS, but its structure can be read anywhere: header, segments, entry point,
dependencies, binds, the TLS descriptor and the rewrite of %fs accesses."""

from pathlib import Path
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent / "macos"))

from import_fixture import fixture as import_fixture
from nid import nid
from tls_fixture import fixture as tls_fixture
from test_guest_module_directories import module_with_symbol

MH_MAGIC_64 = 0xFEEDFACF
CPU_TYPE_X86_64 = 0x01000007
MH_EXECUTE = 2
MH_PIE = 0x200000
LC_SEGMENT_64 = 0x19
LC_LOAD_DYLIB = 0xC
LC_RPATH = 0x8000001C
LC_DYLD_INFO_ONLY = 0x80000022
LC_MAIN = 0x80000028
PAGE = 0x1000
IMAGE_BASE = 0x140000000
META_VERSION = 2
PROT_EXEC = 4
FS_LOAD = bytes.fromhex("64488b0425" "00000000")
GS_LOAD = bytes.fromhex("65488b04c5" "00000000")


class MachO:
    def __init__(self, data):
        self.data = data
        magic, cputype, _, filetype, ncmds, sizeofcmds, flags, _ = struct.unpack_from("<IIIIIIII", data, 0)
        self.magic, self.cputype, self.filetype, self.sizeofcmds, self.flags = magic, cputype, filetype, sizeofcmds, flags
        self.segments = []
        self.sections = {}
        self.dylibs = []
        self.rpaths = []
        self.entryoff = None
        self.dyld_info = None
        offset = 32
        for _ in range(ncmds):
            cmd, size = struct.unpack_from("<II", data, offset)
            if cmd == LC_SEGMENT_64:
                name = data[offset + 8:offset + 24].rstrip(b"\0").decode()
                vmaddr, vmsize, fileoff, filesize, maxprot, initprot, nsects = struct.unpack_from("<QQQQiiI", data, offset + 24)
                self.segments.append(dict(name=name, vmaddr=vmaddr, vmsize=vmsize, fileoff=fileoff, filesize=filesize, maxprot=maxprot, initprot=initprot))
                for index in range(nsects):
                    section = offset + 72 + index * 80
                    sectname = data[section:section + 16].rstrip(b"\0").decode()
                    addr, sectsize, fileoffset = struct.unpack_from("<QQI", data, section + 32)
                    self.sections[(name, sectname)] = (addr, sectsize, fileoffset)
            elif cmd in (LC_LOAD_DYLIB, LC_RPATH):
                text = data[offset + struct.unpack_from("<I", data, offset + 8)[0]:offset + size].split(b"\0")[0].decode()
                (self.dylibs if cmd == LC_LOAD_DYLIB else self.rpaths).append(text)
            elif cmd == LC_MAIN:
                self.entryoff = struct.unpack_from("<Q", data, offset + 8)[0]
            elif cmd == LC_DYLD_INFO_ONLY:
                self.dyld_info = struct.unpack_from("<10I", data, offset + 8)
            offset += size
        self.end = offset

    def segment(self, name):
        return next(segment for segment in self.segments if segment["name"] == name)

    def executable_ranges(self):
        return [(s["fileoff"], s["fileoff"] + s["filesize"]) for s in self.segments if s["initprot"] & PROT_EXEC]


def relink(relinker, directory, name, image):
    source = Path(directory) / f"{name}.elf"
    output = Path(directory) / name
    source.write_bytes(bytes(image))
    result = subprocess.run([str(relinker), "--macos", "--skip-sce-module", str(source), str(output)], capture_output=True, text=True, timeout=120)
    if result.returncode != 0:
        raise SystemExit(f"{name}: relinker failed: {result.stdout}{result.stderr}")
    return output.read_bytes()


def check(condition, message):
    if not condition:
        raise SystemExit(message)


def check_structure(name, data):
    macho = MachO(data)
    check(macho.magic == MH_MAGIC_64, f"{name}: not a 64-bit Mach-O")
    check(macho.cputype == CPU_TYPE_X86_64, f"{name}: not x86-64")
    check(macho.filetype == MH_EXECUTE, f"{name}: not an executable")
    check(macho.flags & MH_PIE == 0, f"{name}: MH_PIE is set, so dyld slides the fixed-base image")
    check(macho.end == 32 + macho.sizeofcmds, f"{name}: load commands do not add up to sizeofcmds")
    names = [segment["name"] for segment in macho.segments]
    check(names[:2] == ["__PAGEZERO", "__TEXT"] and names[-1] == "__LINKEDIT", f"{name}: unexpected segment order {names}")
    check(any(n.startswith("__ELF") for n in names) and "__APS5DATA" in names, f"{name}: image segments missing")
    zero = macho.segment("__PAGEZERO")
    check(zero["vmaddr"] == 0 and zero["vmsize"] == IMAGE_BASE and zero["initprot"] == 0, f"{name}: bad __PAGEZERO")
    expected = IMAGE_BASE
    for segment in macho.segments[1:]:
        check(segment["vmaddr"] == expected and segment["vmaddr"] % PAGE == 0 and segment["vmsize"] % PAGE == 0, f"{name}: {segment['name']} is not contiguous and page aligned")
        check(segment["fileoff"] + segment["filesize"] <= len(data), f"{name}: {segment['name']} runs past the end of the file")
        expected += segment["vmsize"]
    linkedit = macho.segment("__LINKEDIT")
    check(linkedit["initprot"] == 1 and linkedit["fileoff"] + linkedit["filesize"] == len(data), f"{name}: __LINKEDIT does not end the file")
    check(macho.entryoff is not None and any(lo <= macho.entryoff < hi for lo, hi in macho.executable_ranges()), f"{name}: entry point is outside the executable segments")
    check("@rpath/libkernel.prx" in macho.dylibs and "/usr/lib/libSystem.B.dylib" in macho.dylibs, f"{name}: dependencies {macho.dylibs}")
    check(macho.rpaths == ["@executable_path/libs"], f"{name}: run paths {macho.rpaths}")
    check(macho.dyld_info is not None, f"{name}: no binding information")
    return macho


def check_main_entry_mxcsr(macho, data):
    stub = data[macho.entryoff:]
    setup = bytes.fromhex("4883ec100fae1c24c7442404c09f00000fae542404")
    call = stub.find(bytes.fromhex("ff15"))
    check(call >= len(setup), "entry stub does not initialize guest MXCSR before its start call")
    check(stub[call - len(setup):call] == setup, "entry stub does not save host MXCSR and load guest MXCSR 0x9fc0")
    entry = stub.find(bytes.fromhex("488d3d"))
    entry_target = IMAGE_BASE + macho.entryoff + entry + 7 + struct.unpack_from("<i", stub, entry + 3)[0]
    check(any(segment["initprot"] & PROT_EXEC and segment["vmaddr"] <= entry_target < segment["vmaddr"] + segment["vmsize"] for segment in macho.segments),
          "entry stub does not point at executable guest code")
    start_target = IMAGE_BASE + macho.entryoff + call + 6 + struct.unpack_from("<i", stub, call + 2)[0]
    meta_segment = macho.segment("__APS5DATA")
    check(meta_segment["vmaddr"] <= start_target < meta_segment["vmaddr"] + meta_segment["vmsize"], "entry stub start call does not point at its bound start slot")
    restore = bytes.fromhex("0fae14244883c4100f0b")
    check(stub[call + 6:call + 6 + len(restore)] == restore, "entry stub does not restore host MXCSR after the guest start call")


def check_imports(data):
    macho = check_structure("import", data)
    check_main_entry_mxcsr(macho, data)
    check("@rpath/libc.prx" in macho.dylibs, f"import: libc.prx is not a dependency: {macho.dylibs}")
    linkedit = macho.segment("__LINKEDIT")
    binds = data[linkedit["fileoff"]:linkedit["fileoff"] + linkedit["filesize"]]
    for symbol in ("puts", "exit"):
        check(("_" + nid(symbol)).encode() in binds, f"import: {symbol} is not bound by its NID")
    code = b"".join(data[lo:hi] for lo, hi in macho.executable_ranges())
    check(bytes.fromhex("4883ec08") in code, "import: the guest code is missing from the image")


def check_runtime_dependencies(relinker, directory):
    for needed, expected in (
            (b"libc.prx", ["@rpath/libc.prx", "@rpath/libkernel.prx"]),
            (b"libkernel.prx", ["@rpath/libkernel.prx", "@rpath/libc.prx"]),
            (b"libSceLibcInternal.prx", ["@rpath/libSceLibcInternal.prx", "@rpath/libkernel.prx", "@rpath/libc.prx"]),
            (b"libSceVideoOut.prx", ["@rpath/libSceVideoOut.prx", "@rpath/libkernel.prx", "@rpath/libc.prx"])):
        name = needed.decode().removesuffix(".prx")
        dylibs = [dylib for dylib in check_structure(name, relink(relinker, directory, name, import_fixture(library=needed))).dylibs if dylib.startswith("@rpath/")]
        check(dylibs == expected, f"{name}: dependencies {dylibs}, expected {expected}")


def check_tls(data):
    check(FS_LOAD in bytes(tls_fixture()), "tls: the fixture no longer loads through %fs")
    macho = check_structure("tls", data)
    address, size, offset = macho.sections[("__APS5DATA", "__meta")]
    meta = struct.unpack_from("<12Q", data, offset)
    check(meta[0] == META_VERSION, f"tls: metadata version {meta[0]}")
    version, template, template_size, block_size, alignment = meta[4], meta[5], meta[6], meta[7], meta[8]
    check(version == 1 and template_size == 8 and block_size == 16 and alignment == 8, f"tls: descriptor {meta[4:10]}")
    check(IMAGE_BASE <= template < IMAGE_BASE + sum(s["vmsize"] for s in macho.segments), "tls: template is outside the image")
    code = b"".join(data[lo:hi] for lo, hi in macho.executable_ranges())
    check(FS_LOAD not in code, "tls: an %fs access was left in the code")
    check(GS_LOAD in code, "tls: the thread pointer is not read through a pthread key")


def check_options(relinker, directory):
    source = Path(directory) / "conflict.elf"
    source.write_bytes(bytes(import_fixture()))
    result = subprocess.run([str(relinker), "--macos", "--windows", "--skip-sce-module", str(source), str(Path(directory) / "conflict")], capture_output=True, text=True, timeout=120)
    check(result.returncode != 0 and "--windows conflicts with --macos" in result.stdout + result.stderr, "--macos together with --windows was accepted")
    lazy = Path(directory) / "lazy"
    result = subprocess.run([str(relinker), "--macos", "--lazy-binding", "--skip-sce-module", str(source), str(lazy)], capture_output=True, text=True, timeout=120)
    check(result.returncode == 2 and "macOS target does not support --lazy-binding" in result.stderr, f"--macos with --lazy-binding: exit {result.returncode}: {result.stdout}{result.stderr}")
    check(not lazy.exists(), "--macos with --lazy-binding created an output")


def check_runpath_lists(relinker, directory):
    source = Path(directory) / "runpaths.elf"
    source.write_bytes(bytes(import_fixture()))
    modules = Path(directory) / "sce_module"
    modules.mkdir()
    (modules / "module.prx").write_bytes(module_with_symbol(True))
    for index, (configured, expected) in enumerate((
            ("$ORIGIN/first:$ORIGIN/second", ["@executable_path/first", "@executable_path/second"]),
            ("$ORIGIN:$ORIGIN/libs:$ORIGIN", ["@executable_path", "@executable_path/libs", "@executable_path"]),
            ("/first:/second", ["/first", "/second"]))):
        output = Path(directory) / f"runpaths-{index}"
        options = ["--skip-sce-module"] if configured.startswith("/") else []
        result = subprocess.run([str(relinker), "--macos", *options, "--rpath", configured,
                                 str(source), str(output)], capture_output=True, text=True, timeout=20)
        check(result.returncode == 0, f"run paths: {result.stdout}{result.stderr}")
        actual = MachO(output.read_bytes()).rpaths
        check(actual == expected, f"run paths: {actual}")
        if not options:
            guest = MachO((Path(directory) / "app0/sce_module/module.prx.guest.prx").read_bytes()).rpaths
            expected_guest = [path.replace("@executable_path", "@loader_path/../..") for path in expected]
            check(guest == expected_guest, f"guest run paths: {guest}")
    for configured in (":$ORIGIN/libs", "$ORIGIN/libs:", "$ORIGIN/first::$ORIGIN/second"):
        output = Path(directory) / "empty-runpaths"
        result = subprocess.run([str(relinker), "--macos", "--skip-sce-module", "--rpath", configured,
                                 str(source), str(output)], capture_output=True, text=True, timeout=20)
        check(result.returncode != 0 and "Empty macOS run path component" in result.stdout + result.stderr,
              f"empty run path accepted: {configured}")
        check(not output.exists(), "empty run path created an output")


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-macos-output-") as directory:
        check_imports(relink(relinker, directory, "import", import_fixture()))
        check_runtime_dependencies(relinker, directory)
        check_tls(relink(relinker, directory, "tls", tls_fixture()))
        check_options(relinker, directory)
        check_runpath_lists(relinker, directory)
    print("macOS output checks passed")


if __name__ == "__main__":
    main()
