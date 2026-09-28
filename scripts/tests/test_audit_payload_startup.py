import importlib.util
import pathlib
import struct
import unittest


SCRIPT = pathlib.Path(__file__).resolve().parents[1] / "audit-payload-startup.py"
SPEC = importlib.util.spec_from_file_location("payload_audit", SCRIPT)
audit = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(audit)


def elf_fixture():
    data = bytearray(0x280)
    data[:16] = b"\x7fELF\x02\x01\x01\x09" + bytes(8)
    struct.pack_into("<HHIQQQ", data, 16, 3, 62, 1, 0x40, 64, 0x80)
    struct.pack_into("<HHHH", data, 54, 56, 1, 64, 3)
    struct.pack_into("<IIQQQQQQ", data, 64, 1, 5, 0x200, 0x40, 0, 0x20, 0x20, 0x1000)
    data[0x200:0x206] = b"\x55\x48\x89\xe5\x0f\x05"
    names = b"\0payload_args\0__crt_syscall_init\0__kernel_init\0"
    data[0x220:0x220 + len(names)] = names
    struct.pack_into("<IIQQQQIIQQ", data, 0x80 + 64, 0, 3, 0, 0, 0x220, len(names), 0, 0, 1, 0)
    struct.pack_into("<IIQQQQIIQQ", data, 0x80 + 128, 0, 2, 0, 0, 0x160, 96, 1, 0, 8, 24)
    for index, name in enumerate((b"payload_args", b"__crt_syscall_init", b"__kernel_init"), 1):
        struct.pack_into("<IBBHQQ", data, 0x160 + index * 24, names.index(name), 0, 0, 1, 0x40, 8)
    return data


class StartupAuditTests(unittest.TestCase):
    def test_sdk_contract_and_syscall_opcode(self):
        result = audit.inspect(elf_fixture())
        self.assertTrue(result["sdk_payload_detected"])
        self.assertEqual(result["raw_syscall_opcode_candidates"], ["0x44"])
        self.assertFalse(result["host_windows_runnable"])

    def test_truncated_segment_rejected(self):
        data = elf_fixture()
        struct.pack_into("<Q", data, 64 + 32, len(data))
        with self.assertRaises(audit.ElfError):
            audit.inspect(data)

    def test_non_elf_rejected(self):
        with self.assertRaises(audit.ElfError):
            audit.inspect(bytes(64))


if __name__ == "__main__":
    unittest.main()
