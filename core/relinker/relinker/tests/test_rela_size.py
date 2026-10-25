from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_dynamic_segment import fixture


def main():
    relinker = Path(sys.argv[1]).resolve()
    failures = []
    with tempfile.TemporaryDirectory(prefix="anyps5-rela-size-") as directory:
        work = Path(directory)
        for tag in (8, 0x61000031):
            for size in (0, 1, 23, 24, 25, 47, 48, 0xffffffffffffffff):
                image = fixture()
                struct.pack_into("<qQ", image, 0x450, tag, size)
                struct.pack_into("<QQq", image, 0x718, 0x308, 8, 0x210)
                source = work / f"{tag:x}-{size}.elf"
                source.write_bytes(image)
                for mode in ([], ["--windows"]):
                    output = source.with_suffix(".exe" if mode else ".out")
                    result = subprocess.run([str(relinker), "--skip-sce-module", *mode, str(source), str(output)],
                                            capture_output=True, text=True, timeout=20)
                    if size % 24 != 0:
                        valid = result.returncode == 2 and "Invalid DT_RELASZ value" in result.stderr and not output.exists()
                    else:
                        magic = b"MZ" if mode else b"\x7fELF"
                        valid = result.returncode == 0 and output.exists() and output.read_bytes().startswith(magic)
                        if valid and not mode:
                            data = output.read_bytes()
                            header_offset = struct.unpack_from("<Q", data, 32)[0]
                            header_size, header_count = struct.unpack_from("<HH", data, 54)
                            dynamic = next(struct.unpack_from("<IIQQQQQQ", data, header_offset + index * header_size)
                                           for index in range(header_count)
                                           if struct.unpack_from("<I", data, header_offset + index * header_size)[0] == 2)
                            tags = dict(struct.unpack_from("<QQ", data, offset)
                                        for offset in range(dynamic[2], dynamic[2] + dynamic[5], 16))
                            valid = tags.get(8, 0) == size
                    if not valid:
                        failures.append((tag, size, mode, result.returncode, result.stdout, result.stderr))

        def verify_failure(name, image, expected_error, options=("unused-filter=1",)):
            source = work / f"{name}.elf"
            output = work / f"{name}.out"
            source.write_bytes(image)
            result = subprocess.run([str(relinker), "--skip-sce-module", *options, str(source), str(output)],
                                    capture_output=True, text=True, timeout=20)
            if result.returncode != 2 or expected_error not in result.stderr or output.exists():
                failures.append((name, result.returncode, result.stdout, result.stderr))

        neg_image = fixture()
        struct.pack_into("<q", neg_image, 0x700 + 16, -1)
        verify_failure("negative-relative-addend", neg_image, "Relative relocation target address is negative")

        conflict_slot_image = fixture()
        for offset in range(0x400, 0x470, 16):
            if struct.unpack_from("<q", conflict_slot_image, offset)[0] == 8:
                struct.pack_into("<Q", conflict_slot_image, offset + 8, 48)
        struct.pack_into("<QQq", conflict_slot_image, 0x718, 0x300, 8, 0x220)
        verify_failure("conflicting-relative-targets", conflict_slot_image, "Conflicting relative relocation targets for slot")

        mismatched_image = fixture()
        for offset in range(0x400, 0x470, 16):
            if struct.unpack_from("<q", mismatched_image, offset)[0] == 8:
                struct.pack_into("<q", mismatched_image, offset, 0x61000031)
        verify_failure("mismatched-tag-variants", mismatched_image, "Mismatched DT_RELA and DT_RELASZ tag variants")

        both_rela_image = fixture()
        for offset in range(0x400, 0x470, 16):
            if struct.unpack_from("<q", both_rela_image, offset)[0] == 9:
                struct.pack_into("<q", both_rela_image, offset, 0x6100002F)
        verify_failure("both-rela-variants", both_rela_image, "Both DT_OS_ and DT_ variants present for DT_RELA")
    if failures:
        raise AssertionError(failures)
    print("RELA size tests passed")


if __name__ == "__main__":
    main()
