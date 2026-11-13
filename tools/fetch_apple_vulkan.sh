#!/bin/bash
set -euo pipefail
dest=${1:?destination}
arch=${2:?architecture}
moltenvk_version=v1.4.2
loader_version=v1.4.321
stamp="$dest/stamp"
want="$moltenvk_version $loader_version $arch"
if [ -f "$stamp" ] && [ "$(cat "$stamp")" = "$want" ] && [ -f "$dest/lib/libMoltenVK.dylib" ] && [ -f "$dest/lib/libvulkan.dylib" ] && [ -f "$dest/share/vulkan/icd.d/MoltenVK_icd.json" ]; then
    exit 0
fi
mkdir -p "$dest/lib" "$dest/src" "$dest/share/vulkan/icd.d"
archive="$dest/src/MoltenVK-macos.tar"
member=MoltenVK/MoltenVK/dynamic/dylib/macOS/libMoltenVK.dylib
echo "downloading MoltenVK $moltenvk_version"
curl -fsSL --retry 3 -o "$archive" "https://github.com/KhronosGroup/MoltenVK/releases/download/${moltenvk_version}/MoltenVK-macos.tar"
rm -rf "$dest/src/vk-package"
mkdir -p "$dest/src/vk-package"
tar -xf "$archive" -C "$dest/src/vk-package"
cp "$dest/src/vk-package/$member" "$dest/lib/libMoltenVK.dylib"
rm -rf "$dest/src/vk-package" "$archive"
python3 - "$dest/lib/libMoltenVK.dylib" <<'PY'
import struct, sys
data = open(sys.argv[1], "rb").read(4096)
magic, = struct.unpack_from(">I", data, 0)
names = {0x01000007: "x86_64", 0x0100000C: "arm64"}
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
loader_archive="$dest/src/Vulkan-Loader.tar.gz"
loader_src="$dest/src/Vulkan-Loader-${loader_version#v}"
if [ ! -f "$loader_src/CMakeLists.txt" ]; then
    echo "downloading Vulkan-Loader $loader_version"
    curl -fsSL --retry 3 -o "$loader_archive" "https://github.com/KhronosGroup/Vulkan-Loader/archive/refs/tags/${loader_version}.tar.gz"
    tar -xzf "$loader_archive" -C "$dest/src"
    rm -f "$loader_archive"
fi
path_prefix="$dest/src/bin"
mkdir -p "$path_prefix"
printf '#!/bin/sh\nexec python3 "$@"\n' > "$path_prefix/python"
chmod +x "$path_prefix/python"
echo "building Vulkan-Loader $loader_version for $arch"
PATH="$path_prefix:$PATH" cmake -S "$loader_src" -B "$dest/src/loader-build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="$arch" \
    -DUPDATE_DEPS=ON \
    -DBUILD_TESTS=OFF
PATH="$path_prefix:$PATH" cmake --build "$dest/src/loader-build" --target vulkan
python3 - "$dest/src/loader-build" "$dest/lib/libvulkan.dylib" <<'PY'
import os, shutil, struct, sys
root, dest = sys.argv[1:]
found = []
for dirpath, _, names in os.walk(root):
    for name in names:
        if name.startswith("libvulkan") and name.endswith(".dylib") and not os.path.islink(os.path.join(dirpath, name)):
            found.append(os.path.join(dirpath, name))
if not found:
    raise SystemExit("Vulkan-Loader build produced no libvulkan dylib")
def score(path):
    data = open(path, "rb").read(4096)
    magic, = struct.unpack_from(">I", data, 0)
    cpus = []
    if magic in (0xCAFEBABE, 0xBEBAFECA):
        count, = struct.unpack_from(">I", data, 4)
        for index in range(count):
            cpus.append(struct.unpack_from(">I", data, 8 + index * 20)[0])
    elif magic == 0xFEEDFACF:
        cpus.append(struct.unpack_from("<I", data, 4)[0])
    return (0x01000007 in cpus, os.path.getsize(path))
found.sort(key=score)
choice = found[-1]
print("libvulkan.dylib from", choice)
shutil.copyfile(choice, dest)
PY
python3 - "$dest/share/vulkan/icd.d/MoltenVK_icd.json" "$dest/lib/libMoltenVK.dylib" <<'PY'
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
chmod 755 "$dest/lib/libMoltenVK.dylib" "$dest/lib/libvulkan.dylib"
printf '%s\n' "$want" > "$stamp"
echo "apple vulkan ready in $dest"
