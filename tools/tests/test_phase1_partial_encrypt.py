"""Mixed-entropy SELF container: characterization of the entropy heuristic.

This fixture puts one known plaintext PT_LOAD and one high-entropy payload in a
synthetic SELF container and runs the real unwrap_self.py. The current
check_decrypted() only refuses when every load segment is above the entropy
threshold (unwrap_self.py:258-269), so a mixed container is accepted.

The high-entropy payload is deliberately a plaintext byte pattern with a flat
histogram, not ciphertext. High entropy does not prove encryption and random
plaintext is not a ciphertext oracle. This test therefore characterizes the
heuristic; it does not require a refusal and cannot certify that a partially
encrypted SELF is detected. A metadata-based conformance fixture is a separate
gap.
"""

import struct
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import unwrap_self  # noqa: E402

SELF_MAGIC = unwrap_self.SELF_MAGIC
ELF_HEADER_OFFSET = 0x1A0
FIRST_REGION = 0x400
PT_LOAD = unwrap_self.PT_LOAD
PT_DYNAMIC = unwrap_self.PT_DYNAMIC


def build_elf(segments, program_offset=0x40):
    header = bytearray(b"\x7fELF\x02\x01\x01\x09" + b"\x00" * 56)
    struct.pack_into("<HHIQQQIHHHHHH", header, 0x10, 0xFE10, 0x3E, 1, 0x70, program_offset,
                     0, 0, 0x40, 56, len(segments), 0x40, 0, 0)
    body = bytearray(header)
    body.extend(b"\x00" * (program_offset - len(header)))
    for kind, flags, offset, vaddr, size, align in segments:
        body.extend(struct.pack("<IIQQQQQQ", kind, flags, offset, vaddr, vaddr, size, size, align))
    return bytes(body)


def build_container(segments, records, payloads, magic=SELF_MAGIC, inner_elf=True, program_offset=0x40):
    size = max([FIRST_REGION] + [offset + len(block) for offset, block in payloads.items()])
    data = bytearray(size)
    struct.pack_into("<I", data, 0, magic)
    for index, (flags, offset, block_size) in enumerate(records):
        struct.pack_into("<QQQQ", data, unwrap_self.RECORD_OFFSET + index * unwrap_self.RECORD_SIZE,
                         flags, offset, block_size, block_size)
    if inner_elf:
        elf = build_elf(segments, program_offset)
        data[ELF_HEADER_OFFSET:ELF_HEADER_OFFSET + len(elf)] = elf
    for offset, block in payloads.items():
        data[offset:offset + len(block)] = block
    return bytes(data)


def plaintext_segment():
    """A low-entropy, known payload."""
    return b"\x40\x41\x42\x43" * 0x40


def high_entropy_plaintext():
    """A plaintext byte pattern with a flat histogram, not ciphertext."""
    return bytes(range(256))


def main():
    unwrap_self_py = Path(unwrap_self.__file__).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-partial-encrypt-") as directory:
        work = Path(directory)
        source = work / "input.self"
        output = work / "output.elf"
        plaintext = plaintext_segment()
        high_entropy = high_entropy_plaintext()
        assert unwrap_self.entropy(high_entropy) >= unwrap_self.ENTROPY_LIMIT
        segments = [
            (PT_LOAD, 1, 0x1234, 0x0, len(plaintext), 0x1000),
            (PT_LOAD, 4, 0x1500, 0x1000, len(high_entropy), 0x1000),
            (PT_DYNAMIC, 6, 0x1540, 0x1040, 0x10, 8),
        ]
        records = [(0x2804, 0x800, len(plaintext)),
                   (0x310004, 0x910, 0x20),
                   (0x102804, 0xA00, len(high_entropy))]
        payloads = {0x800: plaintext, 0xA00: high_entropy}
        container = build_container(segments, records, payloads)
        source.write_bytes(container)

        result = subprocess.run(
            [sys.executable, str(unwrap_self_py), str(source), str(output)],
            capture_output=True, text=True, timeout=30)
        # Current heuristic accepts a mixed container. This is not a proof that
        # the high-entropy segment is encrypted or that no refusal is required.
        assert result.returncode == 0, (result.returncode, result.stderr)
        assert output.exists(), "the unwrap must emit an ELF for this mixed container"
        data = output.read_bytes()
        phoff = struct.unpack_from("<Q", data, 0x20)[0]
        phnum = struct.unpack_from("<H", data, 0x38)[0]
        payloads_out = []
        for index in range(phnum):
            kind, flags, offset, vaddr, _, size, _, _ = struct.unpack_from(
                "<IIQQQQQQ", data, phoff + index * 56)
            if kind == PT_LOAD:
                payloads_out.append((offset, data[offset:offset + size]))
        assert any(payload == plaintext for _, payload in payloads_out), \
            "the plaintext segment must survive byte-for-byte"
        assert any(payload == high_entropy for _, payload in payloads_out), \
            "the high-entropy plaintext segment must survive byte-for-byte"
        print("Mixed-entropy heuristic characterized (acceptance; not ciphertext proof)")


if __name__ == "__main__":
    main()
