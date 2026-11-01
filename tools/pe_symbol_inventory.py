import argparse
import json
import sys
from pathlib import Path
import struct

from symbolize_samples import PeModule


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Inventory the existing sampler parser's PE code symbols and identity metadata without reading samples")
    parser.add_argument("module", type=Path)
    args = parser.parse_args(argv)
    try:
        module = PeModule(args.module)
        code = [{"rva": start, "end": end, "symbols": [{"rva": address, "name": name}
                 for address, name in zip(addresses, names)]} for start, end, addresses, names in module.code]
    except (OSError, ValueError, struct.error) as error:
        parser.error(str(error))
    print(json.dumps({"image_base": module.image_base, "size": module.size, "timestamp": module.timestamp,
                      "symbol_source": "exports" if module.exported else "coff", "code": code}, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
