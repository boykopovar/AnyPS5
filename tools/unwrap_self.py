#!/usr/bin/env python3
"""unwrap_self -- turn a decrypted SELF or fSELF container into a clean ELF.

The relinker reads a plain ELF executable and skips SELF containers with a warning,
so an eboot.bin or a *.prx taken from a title has to be unwrapped first. Two things
differ inside a container:

  - The inner ELF program headers carry stale offsets. A SELF inserts its own
    metadata blocks between the segments, so the segment data sits at the offsets
    listed in the SELF segment-record table at file offset 0x40, records of 0x20
    bytes holding flags, data offset, size and the size again. Segments the table
    does not cover are placed by the vaddr to file mapping of the PT_LOAD that
    contains them; segments without a vaddr carry the shift of the last mapped one.
  - The segments are packed without honouring p_align, while ELF requires
    p_offset % p_align == p_vaddr % p_align. Every region is copied to an offset
    that satisfies it, which the relinker enforces on guest modules.

Only decrypted containers are handled. A container whose load segments all look
encrypted is refused instead of copied: the entropy of each load segment is
measured and a run of segments near eight bits per byte is reported.

usage: unwrap_self.py <input.self> <output.elf> [--verbose]
"""

import argparse
import math
import struct
import sys
from collections import Counter
from pathlib import Path

SELF_MAGIC = 0x1D3D154F
SELF_MAGIC_KERNEL = 0xEEF51454
RECORD_OFFSET = 0x40
RECORD_SIZE = 0x20
FIRST_REGION = 0x400
PT_LOAD = 1
ENTROPY_LIMIT = 7.5
ENTROPY_SAMPLE = 0x10000

PHDR_NAMES = {
    PT_LOAD: "PT_LOAD",
    2: "PT_DYNAMIC",
    3: "PT_INTERP",
    4: "PT_NOTE",
    6: "PT_PHDR",
    7: "PT_TLS",
    0x6474E550: "PT_GNU_EH_FRAME",
    0x6474E551: "PT_GNU_STACK",
    0x6474E552: "PT_GNU_RELRO",
    0x61000000: "PT_SCE_DYNLIBDATA",
    0x61000001: "PT_OS_PROCPARAM",
    0x61000002: "PT_OS_0x61000002",
    0x6FFFFF00: "PT_OS_0x6fffff00",
    0x6FFFFF01: "PT_OS_0x6fffff01",
}


class Refused(Exception):
    pass


def entropy(data):
    if not data:
        return 0.0
    counts = Counter(data)
    length = len(data)
    return -sum((count / length) * math.log2(count / length) for count in counts.values())


def read_records(data):
    records = []
    offset = RECORD_OFFSET
    while offset + RECORD_SIZE <= FIRST_REGION:
        flags, segment, size, size_again = struct.unpack_from("<QQQQ", data, offset)
        if size == 0 or size != size_again:
            break
        if segment >= len(data) or segment < 0x350 or segment % 0x10 != 0:
            break
        if size > len(data):
            break
        records.append((flags, segment, size))
        offset += RECORD_SIZE
    return records


def align_up(value, alignment):
    if alignment <= 1:
        return value
    return (value + alignment - 1) // alignment * alignment


def parse_elf(data):
    inner = data.find(b"\x7fELF")
    if inner < 0:
        raise Refused("no inner ELF header")
    header = bytearray(data[inner:inner + 0x40])
    program_offset = struct.unpack_from("<Q", header, 0x20)[0]
    entry_size = struct.unpack_from("<H", header, 0x36)[0]
    count = struct.unpack_from("<H", header, 0x38)[0]
    if entry_size != 56 or count == 0:
        raise Refused("program header table is %d entries of %d bytes" % (count, entry_size))
    headers = []
    for index in range(count):
        offset = inner + program_offset + index * 56
        kind, flags, file_offset, vaddr, _, file_size, memory_size, alignment = struct.unpack_from(
            "<IIQQQQQQ", data, offset)
        headers.append({
            "index": index, "type": kind, "flags": flags, "offset": file_offset,
            "vaddr": vaddr, "size": file_size, "memory": memory_size,
            "align": alignment, "source": file_offset, "host": None,
        })
    return inner, header, headers


def place_segments(data, headers, records):
    loads = [header for header in headers if header["type"] == PT_LOAD]
    used = set()
    for header in loads:
        if header["size"] == 0:
            continue
        for index, (_, segment, size) in enumerate(records):
            if index in used:
                continue
            if size == header["size"]:
                used.add(index)
                header["source"] = segment
                break

    def load_for(vaddr, size):
        found = None
        for header in loads:
            end = header["vaddr"] + header["memory"]
            if header["memory"] and header["vaddr"] <= vaddr and (vaddr < end or (size == 0 and vaddr == end)):
                found = header
        return found

    tail = None
    for header in headers:
        if header["type"] == PT_LOAD:
            continue
        if header["vaddr"] != 0:
            host = load_for(header["vaddr"], header["size"])
            if host is None:
                raise Refused("segment %d vaddr %#x is in no PT_LOAD" % (header["index"], header["vaddr"]))
            header["host"] = host
            header["source"] = host["source"] + (header["vaddr"] - host["vaddr"])
            if tail is None or header["offset"] > tail[0]:
                tail = (header["offset"], header["source"] - header["offset"])
        elif header["size"]:
            header["source"] = header["offset"] + (tail[1] if tail else 0)
            if header["source"] > len(data):
                header["source"] = len(data)
            keep = max(0, len(data) - header["source"])
            header["size"] = min(header["size"], keep)
            header["memory"] = min(header["memory"], keep)

    for header in headers:
        if header["source"] > len(data) or header["size"] > len(data) - header["source"]:
            raise Refused("segment %d does not fit the file" % header["index"])


def check_decrypted(data, headers):
    encrypted = []
    for header in headers:
        if header["type"] != PT_LOAD or header["size"] == 0:
            continue
        sample = data[header["source"]:header["source"] + min(header["size"], ENTROPY_SAMPLE)]
        value = entropy(sample)
        if value >= ENTROPY_LIMIT:
            encrypted.append((header["index"], header["source"], value))
    if encrypted and len(encrypted) == len([h for h in headers if h["type"] == PT_LOAD and h["size"]]):
        listed = ", ".join("segment %d at %#x (%.2f bits per byte)" % item for item in encrypted)
        raise Refused("every load segment looks encrypted: " + listed)


def unwrap(path, output, verbose=False):
    data = bytes(Path(path).read_bytes())
    if len(data) < FIRST_REGION:
        raise Refused("file is smaller than the SELF header area")
    magic = struct.unpack_from("<I", data, 0)[0]
    if magic not in (SELF_MAGIC, SELF_MAGIC_KERNEL):
        raise Refused("not a SELF container (magic %#x)" % magic)

    records = read_records(data)
    inner, elf_header, headers = parse_elf(data)
    place_segments(data, headers, records)
    check_decrypted(data, headers)

    regions = []
    for header in headers:
        if header["type"] == PT_LOAD and header["size"]:
            regions.append({"old": header["source"], "size": header["size"],
                            "vaddr": header["vaddr"], "align": header["align"]})

    def region_for(offset, size):
        for region in regions:
            if region["old"] <= offset and offset + size <= region["old"] + region["size"]:
                return region
        return None

    for header in headers:
        if header["type"] == PT_LOAD or header["size"] == 0:
            continue
        if region_for(header["source"], header["size"]) is None:
            regions.append({"old": header["source"], "size": header["size"], "vaddr": 0, "align": 1})

    regions.sort(key=lambda region: region["old"])
    cursor = FIRST_REGION
    for region in regions:
        alignment = region["align"] if region["align"] and (region["align"] & (region["align"] - 1)) == 0 else 1
        placed = align_up(cursor, alignment)
        if region["vaddr"] % alignment != placed % alignment:
            placed += (region["vaddr"] % alignment - placed % alignment) % alignment
        region["new"] = placed
        cursor = placed + region["size"]

    result = bytearray(cursor)
    for region in regions:
        result[region["new"]:region["new"] + region["size"]] = data[region["old"]:region["old"] + region["size"]]

    for header in headers:
        if header["size"] == 0 and header["type"] == PT_LOAD:
            header["new"] = 0
            continue
        region = region_for(header["source"], header["size"])
        header["new"] = header["source"] if region is None else region["new"] + (header["source"] - region["old"])
        if header["new"] + header["size"] > len(result):
            raise Refused("segment %d escaped the output" % header["index"])

    result[0:0x40] = elf_header
    struct.pack_into("<Q", result, 0x28, 0)
    struct.pack_into("<H", result, 0x3C, 0)
    struct.pack_into("<H", result, 0x3E, 0)
    program_offset = struct.unpack_from("<Q", elf_header, 0x20)[0]
    for item in headers:
        struct.pack_into("<IIQQQQQQ", result, program_offset + item["index"] * 56,
                         item["type"], item["flags"], item["new"], item["vaddr"], item["vaddr"],
                         item["size"], item["memory"], item["align"])

    if verbose:
        print("%s: %#x bytes, inner ELF at %#x, %d program headers, %d SELF records"
              % (Path(path).name, len(data), inner, len(headers), len(records)))
        for item in headers:
            name = PHDR_NAMES.get(item["type"], "PT_%#x" % item["type"])
            moved = "" if item["new"] == item["offset"] else ", was %#x" % item["offset"]
            print("  [%2d] %-18s vaddr=%#011x filesz=%#010x offset=%#010x%s"
                  % (item["index"], name, item["vaddr"], item["size"], item["new"], moved))

    destination = Path(output)
    if destination.parent != Path(""):
        destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(result)
    return len(result)


def main():
    parser = argparse.ArgumentParser(description="unwrap a decrypted SELF or fSELF container into a clean ELF")
    parser.add_argument("input", help="SELF container to read")
    parser.add_argument("output", help="ELF executable to write")
    parser.add_argument("--verbose", action="store_true", help="print the program header mapping")
    arguments = parser.parse_args()
    try:
        size = unwrap(arguments.input, arguments.output, arguments.verbose)
    except Refused as error:
        print("FAIL: %s: %s" % (arguments.input, error), file=sys.stderr)
        return 2
    except OSError as error:
        print("FAIL: %s: %s" % (arguments.input, error), file=sys.stderr)
        return 2
    print("wrote %s, %d bytes" % (arguments.output, size))
    return 0


if __name__ == "__main__":
    sys.exit(main())
