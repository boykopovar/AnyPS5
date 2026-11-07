#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
build=${1:-$root/build}
guest=$build/macos-guest
rm -rf "$guest"
mkdir -p "$guest/libs"
python3 "$root/core/relinker/relinker/tests/macos/import_fixture.py" "$guest/import.elf"
"$build/core/relinker/relinker" --macos --skip-sce-module "$guest/import.elf" "$guest/eboot"
cp "$build/core/libs/libs/"*.prx "$guest/libs/"
echo "relinked guest"
python3 - "$guest/eboot" <<'PY'
import struct, sys
data = open(sys.argv[1], "rb").read()
ncmds, sizeofcmds = struct.unpack_from("<II", data, 16)
offset = 32
entry = None
for _ in range(ncmds):
    cmd, size = struct.unpack_from("<II", data, offset)
    if cmd == 0x80000028:
        entry = struct.unpack_from("<Q", data, offset + 8)[0]
    offset += size
print(f"entryoff 0x{entry:x} bytes {data[entry:entry+24].hex()}")
PY
set +e
NSUnbufferedIO=YES APS5_TRACE_START=1 APS5_TRACE_EXIT=1 "$guest/eboot"
guest_status=$?
set -e
if [ "$guest_status" -ne 0 ]; then
    echo "guest status $guest_status"
    exit "$guest_status"
fi
echo "guest thread"
"$build/macos_guest_thread_self"
