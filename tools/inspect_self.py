import argparse
import json
import mmap
import os
import struct
import sys
from pathlib import Path

from unwrap_self import FIRST_REGION, SELF_MAGIC, SELF_MAGIC_KERNEL, Refused, parse_elf, read_records


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Inspect SELF record and original inner ELF header metadata as JSON; this does not validate conversion or decrypt segments")
    parser.add_argument("input", type=Path)
    args = parser.parse_args(argv)
    try:
        with args.input.open("rb") as source:
            if os.fstat(source.fileno()).st_size < FIRST_REGION:
                raise Refused("file is smaller than the SELF header area")
            with mmap.mmap(source.fileno(), 0, access=mmap.ACCESS_READ) as data:
                magic = struct.unpack_from("<I", data)[0]
                if magic not in (SELF_MAGIC, SELF_MAGIC_KERNEL):
                    raise Refused("not a SELF container")
                records = read_records(data)
                inner, _, headers = parse_elf(data)
                size = len(data)
    except (OSError, ValueError, Refused, struct.error) as error:
        parser.error(str(error))
    fields = ("index", "type", "flags", "offset", "vaddr", "size", "memory", "align")
    print(json.dumps({"size": size, "magic": magic, "inner_elf": inner,
                      "records": [{"flags": flags, "offset": offset, "size": size} for flags, offset, size in records],
                      "program_headers": [{field: header[field] for field in fields} for header in headers]}, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
