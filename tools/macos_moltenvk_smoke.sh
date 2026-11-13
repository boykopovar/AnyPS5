#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
interactive=0
build=
for argument in "$@"; do
    case $argument in
        --interactive) interactive=1 ;;
        *)
            if [ -n "$build" ]; then
                echo "usage: tools/macos_moltenvk_smoke.sh [--interactive] [build]" >&2
                exit 2
            fi
            build=$argument
            ;;
    esac
done
build=${build:-$root/build}
case $build in
    /*) ;;
    *) build=$root/$build ;;
esac
guest=$build/macos-square
vulkan=$build/vulkan
/bin/bash "$root/tools/fetch_apple_vulkan.sh" "$vulkan" x86_64
rm -rf "$guest"
mkdir -p "$guest/libs" "$guest/vk"
cp "$vulkan/lib/libMoltenVK.dylib" "$guest/vk/libMoltenVK.dylib"
cp "$vulkan/lib/libvulkan.dylib" "$guest/vk/libvulkan.dylib"
cp "$vulkan/lib/libvulkan.dylib" "$guest/vk/libvulkan.1.dylib"
python3 - "$guest/vk/MoltenVK_icd.json" "$guest/vk/libMoltenVK.dylib" <<'PY'
import json, sys
with open(sys.argv[1], "w") as handle:
    json.dump({
        "file_format_version": "1.0.0",
        "ICD": {
            "library_path": sys.argv[2],
            "api_version": "1.2.0",
            "is_portability_driver": True,
        },
    }, handle, indent=2)
    handle.write("\n")
PY
python3 - "$guest/vk/libMoltenVK.dylib" <<'PY'
import struct, sys
path = sys.argv[1]
data = open(path, "rb").read(4096)
magic, = struct.unpack_from(">I", data, 0)
names = {0x00000007: "i386", 0x01000007: "x86_64", 0x0000000C: "arm", 0x0100000C: "arm64"}
cpus = []
if magic in (0xCAFEBABE, 0xBEBAFECA):
    count, = struct.unpack_from(">I", data, 4)
    for index in range(count):
        cputype, = struct.unpack_from(">I", data, 8 + index * 20)
        cpus.append(names.get(cputype, hex(cputype)))
elif magic == 0xFEEDFACF:
    cputype, = struct.unpack_from("<I", data, 4)
    cpus.append(names.get(cputype, hex(cputype)))
else:
    raise SystemExit(f"libMoltenVK.dylib is not a Mach-O file (magic {magic:#x})")
print("libMoltenVK.dylib architectures:", " ".join(cpus))
if "x86_64" not in cpus:
    raise SystemExit("libMoltenVK.dylib has no x86-64 slice")
PY
python3 "$root/core/relinker/relinker/tests/macos/square_elf.py" "$guest/square.elf"
"$build/core/relinker/relinker" --macos --skip-sce-module "$guest/square.elf" "$guest/eboot"
cp "$build/core/libs/libs/"*.prx "$guest/libs/"
mkdir -p "$guest/app0/sce_sys"
cat > "$guest/app0/sce_sys/param.json" <<'EOF'
{"titleId":"SQRE00001","localizedParameters":{"defaultLanguage":"en-US","en-US":{"titleName":"Square"}}}
EOF
echo "relinked square"
cd "$guest"
set +e
if [ "$interactive" -eq 1 ]; then
    echo "interactive: left stick moves the player, Cross clears both coins, a tone plays every 15 frames, Escape or Options closes the window, otherwise it stays open for 30 seconds"
    DYLD_LIBRARY_PATH="$guest/vk${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}" \
        VK_DRIVER_FILES="$guest/vk/MoltenVK_icd.json" \
        VK_ICD_FILENAMES="$guest/vk/MoltenVK_icd.json" \
        APS5_SQUARE_INTERACTIVE=1 \
        APS5_DUMP_FRAMES=0 \
        NSUnbufferedIO=YES \
        "$guest/eboot"
else
    echo "dump directory $guest"
    DYLD_LIBRARY_PATH="$guest/vk${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}" \
        VK_DRIVER_FILES="$guest/vk/MoltenVK_icd.json" \
        VK_ICD_FILENAMES="$guest/vk/MoltenVK_icd.json" \
        APS5_SQUARE_INTERACTIVE=0 \
        APS5_SQUARE_FRAMES=1 \
        APS5_DUMP_FRAMES=1 \
        APS5_DUMP_SCALE=1 \
        APS5_NO_GPU_DUMP=1 \
        APS5_SYNC_FLIP=1 \
        NSUnbufferedIO=YES \
        "$guest/eboot"
fi
guest_status=$?
set -e
if [ "$guest_status" -ne 0 ]; then
    echo "square status $guest_status"
    exit "$guest_status"
fi
if [ "$interactive" -eq 1 ]; then
    echo "square interactive ok"
    exit 0
fi
python3 - "$guest/frame_000.bmp" <<'PY'
import struct, sys
path = sys.argv[1]
data = open(path, "rb").read()
if data[:2] != b"BM":
    raise SystemExit("frame_000.bmp is not a BMP")
offset, = struct.unpack_from("<I", data, 10)
width, height = struct.unpack_from("<Ii", data, 18)
if width != 320 or height != -180:
    raise SystemExit(f"frame extent {width}x{height}, expected 320x-180")
pixels = data[offset:]
expected = width * 180 * 4
if len(pixels) < expected:
    raise SystemExit(f"frame pixels {len(pixels)}, expected {expected}")

def pixel(x, y):
    start = (y * width + x) * 4
    return pixels[start:start + 4]

background = bytes((0x20, 0x18, 0x10, 0xFF))
square = bytes((0x40, 0xE0, 0x40, 0xFF))
player = bytes((0xF0, 0x60, 0x20, 0xFF))
coin = bytes((0x30, 0xD0, 0xF0, 0xFF))
checks = [
    (0, 0, background), (39, 30, background), (40, 30, square), (67, 57, square), (68, 30, background),
    (200, 90, player), (211, 101, player), (212, 90, background),
    (16, 150, coin), (23, 157, coin), (15, 150, background),
    (260, 16, coin), (267, 23, coin), (268, 16, background),
]
for x, y, want in checks:
    got = pixel(x, y)
    if got != want:
        raise SystemExit(f"pixel {x},{y} is {got.hex()} expected {want.hex()}")
print(f"frame_000.bmp {width}x{-height} square ok")
PY
