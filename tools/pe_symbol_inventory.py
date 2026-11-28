import argparse
import errno
import json
import os
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
    try:
        print(json.dumps({"image_base": module.image_base, "size": module.size, "timestamp": module.timestamp,
                          "symbol_source": "exports" if module.exported else "coff", "code": code}, ensure_ascii=False, indent=2))
        sys.stdout.flush()
    except OSError as exc:
        if not (isinstance(exc, BrokenPipeError) or (sys.platform == "win32" and exc.errno == errno.EINVAL)):
            raise
        devnull = os.open(os.devnull, os.O_WRONLY)
        try:
            os.dup2(devnull, sys.stdout.fileno())
        except (AttributeError, OSError, ValueError):
            pass
        finally:
            os.close(devnull)
    return 0


if __name__ == "__main__":
    sys.exit(main())
