import struct
import sys
import unittest
from pathlib import Path

PT_LOAD = 1
PT_DYNAMIC = 2
PT_TLS = 7
DT_NULL = 0
DT_PLTRELSZ = 2
DT_RELA = 7
DT_RELASZ = 8
DT_RELAENT = 9
DT_JMPREL = 23
DT_FLAGS = 30
DF_STATIC_TLS = 0x10
R_X86_64_DTPMOD64 = 16
R_X86_64_TPOFF64 = 18
R_X86_64_TLSDESC = 36

DIRECTORY = None
EXPECT_DESCRIPTORS = False


class Library:
    def __init__(self, path):
        self.path = path
        data = path.read_bytes()
        if data[:4] != b"\x7fELF" or data[4] != 2 or data[5] != 1:
            raise ValueError(f"{path.name} is not a little-endian ELF64 file")
        phoff, = struct.unpack_from("<Q", data, 0x20)
        phentsize, phnum = struct.unpack_from("<HH", data, 0x36)
        loads = []
        dynamic = None
        self.tls_bytes = 0
        for index in range(phnum):
            kind, _, offset, vaddr, _, filesz, memsz, _ = struct.unpack_from("<IIQQQQQQ", data, phoff + index * phentsize)
            if kind == PT_LOAD:
                loads.append((vaddr, offset, filesz))
            elif kind == PT_DYNAMIC:
                dynamic = (offset, filesz)
            elif kind == PT_TLS:
                self.tls_bytes = memsz
        self.flags = 0
        self.relocations = {}
        if dynamic is None:
            return
        entries = {}
        for at in range(dynamic[0], dynamic[0] + dynamic[1], 16):
            tag, value = struct.unpack_from("<qQ", data, at)
            if tag == DT_NULL:
                break
            entries[tag] = value
        self.flags = entries.get(DT_FLAGS, 0)

        def offset_of(address):
            for vaddr, offset, filesz in loads:
                if vaddr <= address < vaddr + filesz:
                    return offset + address - vaddr
            raise ValueError(f"{path.name}: address {address:#x} is outside every loaded segment")

        entry = entries.get(DT_RELAENT, 24)
        for table, size in ((DT_RELA, DT_RELASZ), (DT_JMPREL, DT_PLTRELSZ)):
            if table not in entries or not entries.get(size):
                continue
            start = offset_of(entries[table])
            for at in range(start, start + entries[size], entry):
                info, = struct.unpack_from("<Q", data, at + 8)
                kind = info & 0xFFFFFFFF
                self.relocations[kind] = self.relocations.get(kind, 0) + 1


class PrxTlsModelTest(unittest.TestCase):
    def libraries(self):
        paths = sorted(Path(DIRECTORY).glob("*.prx"))
        self.assertTrue(paths, f"no .prx libraries in {DIRECTORY}")
        return [Library(path) for path in paths]

    def test_no_library_requires_static_tls(self):
        static = [f"{library.path.name} ({library.tls_bytes} bytes of TLS)" for library in self.libraries() if library.flags & DF_STATIC_TLS]
        self.assertEqual(static, [], "these libraries are marked DF_STATIC_TLS, so a dlopen after startup fails once their TLS no longer fits glibc's static surplus: " + ", ".join(static))

    def test_thread_locals_use_descriptors(self):
        if not EXPECT_DESCRIPTORS:
            self.skipTest("the compiler does not take -mtls-dialect=gnu2")
        libraries = self.libraries()
        dynamic = [f"{library.path.name} ({library.relocations[R_X86_64_DTPMOD64]})" for library in libraries if library.relocations.get(R_X86_64_DTPMOD64)]
        self.assertEqual(dynamic, [], "these libraries still reach thread-local storage through __tls_get_addr (R_X86_64_DTPMOD64 relocations): " + ", ".join(dynamic))
        described = [library.path.name for library in libraries if library.relocations.get(R_X86_64_TLSDESC)]
        self.assertTrue(described, "no library reaches its thread-local storage through a TLS descriptor")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        sys.exit("usage: test_prx_tls_model.py <prx directory> <descriptors expected: 0 or 1>")
    DIRECTORY = sys.argv[1]
    EXPECT_DESCRIPTORS = sys.argv[2] not in ("", "0", "OFF", "FALSE")
    unittest.main(argv=sys.argv[:1])
