import struct
import sys
from pathlib import Path

from test_symbol_versions import HASH_OFFSET, SONAME, build_image, patch, symbols


def with_header_field(image, field_offset, value):
    image = bytearray(image)
    struct.pack_into("<I", image, HASH_OFFSET + field_offset, value)
    return bytes(image)


def assert_rejected(patcher, image, expected):
    result, on_disk = patch(patcher, image, SONAME)
    assert result.returncode == 2, (result.returncode, result.stderr)
    message = result.stderr.decode("utf-8", errors="replace")
    assert message.startswith("FAIL: "), message
    assert expected in message, message
    assert result.stdout == b"", result.stdout
    assert on_disk == image, "the rejected library was written back modified"


def test_zero_buckets_is_rejected(patcher):
    image = with_header_field(build_image(symbols(False), False), 0, 0)
    assert_rejected(patcher, image, ".gnu.hash has no buckets")


def test_zero_bloom_size_is_rejected(patcher):
    image = with_header_field(build_image(symbols(False), False), 8, 0)
    assert_rejected(patcher, image, ".gnu.hash has no bloom filter words")


def main():
    patcher = Path(sys.argv[1]).resolve()
    test_zero_buckets_is_rejected(patcher)
    test_zero_bloom_size_is_rejected(patcher)
    print("NID patcher GNU hash tests passed")


if __name__ == "__main__":
    main()
