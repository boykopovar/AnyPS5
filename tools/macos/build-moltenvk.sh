#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/../.." && pwd)
work=${1:-"$repo_root/build-mac/moltenvk-patched"}
work=$(mkdir -p "$work" && cd "$work" && pwd)
marker="$work/.anyps5-moltenvk-work"
if [ -f "$marker" ]; then
	test "$(cat "$marker")" = "AnyPS5 patched MoltenVK build workspace" || {
		echo "build-moltenvk: refusing an unrecognized workspace marker: $work" >&2
		exit 1
	}
elif [ -n "$(find "$work" -mindepth 1 -maxdepth 1 -print -quit)" ]; then
	echo "build-moltenvk: work directory is not empty and is not an AnyPS5 build workspace: $work" >&2
	exit 1
else
	echo "AnyPS5 patched MoltenVK build workspace" > "$marker"
fi
src="$work/MoltenVK"
spvc="$work/SPIRV-Cross"
spvc_build="$work/SPIRV-Cross-build"
stage="$work/stage"

MVK_REPO=https://github.com/utmapp/MoltenVK.git
MVK_COMMIT=05604465d691118cfd20f53a48ecf1aad9c12f93
SPVC_REPO=https://github.com/utmapp/SPIRV-Cross.git
SPVC_COMMIT=939b40b33a44443c404c4078823c406e3c94866f
patch="$script_dir/patches/spirv-cross/0001-MSL-use-Metal-SIMD-group-fence-scope.patch.txt"

if [ "$(uname -s)" != Darwin ]; then
	echo "build-moltenvk: requires macOS and Xcode" >&2
	exit 1
fi
xcrun --find metal >/dev/null
for tool in git cmake ninja python3 lipo arch clang shasum; do
	command -v "$tool" >/dev/null || { echo "build-moltenvk: missing $tool" >&2; exit 1; }
done

fetch_pinned() {
	checkout=$1
	remote=$2
	commit=$3
	clean_mode=${4:-clean}
	if [ ! -d "$checkout/.git" ]; then
		git init -q "$checkout"
		git -C "$checkout" remote add origin "$remote"
	fi
	if ! git -C "$checkout" cat-file -e "$commit^{commit}" 2>/dev/null; then
		git -C "$checkout" fetch -q --depth 1 origin "$commit"
	fi
	git -C "$checkout" checkout -q -f --detach "$commit"
	if [ "$clean_mode" = keep-external ]; then
		git -C "$checkout" clean -q -fdx -e /External
	else
		git -C "$checkout" clean -q -fdx
	fi
}

fetch_pinned "$spvc" "$SPVC_REPO" "$SPVC_COMMIT"
git -C "$spvc" apply --unidiff-zero --check "$patch"
git -C "$spvc" apply --unidiff-zero "$patch"

# Verify the source patch generates Metal's SIMD-group scope before building the ICD.
cmake -S "$spvc" -B "$spvc_build" -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_OSX_ARCHITECTURES=x86_64 \
	-DSPIRV_CROSS_CLI=ON \
	-DSPIRV_CROSS_ENABLE_GLSL=ON \
	-DSPIRV_CROSS_ENABLE_HLSL=ON \
	-DSPIRV_CROSS_ENABLE_CPP=ON \
	-DSPIRV_CROSS_ENABLE_TESTS=OFF \
	-DSPIRV_CROSS_ENABLE_MSL=ON
cmake --build "$spvc_build" --target spirv-cross --parallel

glslang=${GLSLANG_VALIDATOR:-}
if [ -z "$glslang" ]; then
	glslang=$(command -v glslangValidator || true)
fi
if [ -z "$glslang" ] && [ -n "${VULKAN_SDK:-}" ] && [ -x "$VULKAN_SDK/bin/glslangValidator" ]; then
	glslang="$VULKAN_SDK/bin/glslangValidator"
fi
if [ -z "$glslang" ]; then
	echo "build-moltenvk: set GLSLANG_VALIDATOR or install glslangValidator (for example from the Vulkan SDK)" >&2
	exit 1
fi
vulkaninfo=${VULKANINFO:-}
if [ -z "$vulkaninfo" ]; then
	vulkaninfo=$(command -v vulkaninfo || true)
fi
if [ -z "$vulkaninfo" ] && [ -n "${VULKAN_SDK:-}" ] && [ -x "$VULKAN_SDK/bin/vulkaninfo" ]; then
	vulkaninfo="$VULKAN_SDK/bin/vulkaninfo"
fi
if [ -z "$vulkaninfo" ]; then
	echo "build-moltenvk: set VULKANINFO or install vulkaninfo to smoke-test the x86-64 ICD" >&2
	exit 1
fi
if [ -z "${VULKAN_SDK:-}" ] || [ ! -f "$VULKAN_SDK/include/vulkan/vulkan.h" ] || [ ! -e "$VULKAN_SDK/lib/libvulkan.dylib" ]; then
	echo "build-moltenvk: set VULKAN_SDK to a macOS Vulkan SDK with headers and libvulkan.dylib" >&2
	exit 1
fi

fixture="$script_dir/tests/subgroup_memory_barrier.comp"
spv="$work/subgroup_memory_barrier.spv"
msl="$work/subgroup_memory_barrier.msl"
"$glslang" -V --target-env vulkan1.1 -S comp -o "$spv" "$fixture"
"$spvc_build/spirv-cross" "$spv" --msl --msl-version 30200 > "$msl"
grep -q 'thread_scope_simdgroup' "$msl"
if grep -q 'thread_scope_subgroup' "$msl"; then
	echo "build-moltenvk: subgroup barrier still emits the SPIR-V scope spelling" >&2
	exit 1
fi

fetch_pinned "$src" "$MVK_REPO" "$MVK_COMMIT" keep-external
test "$(cat "$src/ExternalRevisions/SPIRV-Cross_repo_revision")" = "$SPVC_COMMIT"
package="$src/Package/Release/MoltenVK"
built="$package/dynamic/dylib/macOS/libMoltenVK.dylib"
build_stamp=$(printf '%s\n' "$MVK_COMMIT" "$SPVC_COMMIT" "$(shasum -a 256 "$patch")" "$(xcodebuild -version)" "$(xcrun --sdk macosx --show-sdk-version)" | shasum -a 256 | cut -d ' ' -f 1)
stamp_file="$work/.moltenvk-build-stamp"
if [ ! -f "$stamp_file" ] || [ "$(cat "$stamp_file")" != "$build_stamp" ] || [ ! -f "$stage/lib/libMoltenVK.dylib" ] || ! lipo -verify_arch x86_64 "$stage/lib/libMoltenVK.dylib" >/dev/null 2>&1; then
	(
		cd "$src"
		env -i PATH="$PATH" HOME="$HOME" LANG="${LANG:-en_US.UTF-8}" \
			./fetchDependencies --macos --spirv-cross-root "$spvc"
	)
	(
		cd "$src"
		env -i PATH="$PATH" HOME="$HOME" LANG="${LANG:-en_US.UTF-8}" make macos
	)
	mkdir -p "$stage/lib" "$stage/share/vulkan/icd.d" "$stage/include" "$stage/licenses"
	lipo "$built" -thin x86_64 -output "$stage/lib/libMoltenVK.dylib"
	cp -R "$package/include/MoltenVK" "$stage/include/"
	cp "$src/LICENSE" "$stage/licenses/MoltenVK-LICENSE"
	cp "$spvc/LICENSE" "$stage/licenses/SPIRV-Cross-LICENSE"
	python3 - "$package/dynamic/dylib/macOS/MoltenVK_icd.json" "$stage/share/vulkan/icd.d/MoltenVK_icd.json" <<'PY'
import json
import pathlib
import sys

source, target = map(pathlib.Path, sys.argv[1:])
manifest = json.loads(source.read_text())
manifest["ICD"]["library_path"] = "../../../lib/libMoltenVK.dylib"
target.write_text(json.dumps(manifest, indent=4) + "\n")
PY
	echo "$build_stamp" > "$stamp_file"
else
	echo "MoltenVK build is current; reusing $stage/lib/libMoltenVK.dylib"
fi

VK_DRIVER_FILES="$stage/share/vulkan/icd.d/MoltenVK_icd.json" arch -x86_64 "$vulkaninfo" --summary > "$work/vulkaninfo.log" 2>&1 || {
	cat "$work/vulkaninfo.log" >&2
	exit 1
}
grep -q 'driverName.*MoltenVK' "$work/vulkaninfo.log" || {
	cat "$work/vulkaninfo.log" >&2
	echo "build-moltenvk: x86-64 Vulkan smoke test did not select MoltenVK" >&2
	exit 1
}
clang -arch x86_64 -std=c11 -Wall -Wextra -Werror \
	-I"$VULKAN_SDK/include" "$script_dir/tests/subgroup_barrier_pipeline.c" \
	-L"$VULKAN_SDK/lib" -lvulkan -Wl,-rpath,"$VULKAN_SDK/lib" \
	-o "$work/subgroup_barrier_pipeline"
VK_DRIVER_FILES="$stage/share/vulkan/icd.d/MoltenVK_icd.json" arch -x86_64 \
	"$work/subgroup_barrier_pipeline" "$spv" > "$work/pipeline.log" 2>&1 || {
	cat "$work/pipeline.log" >&2
	exit 1
}
grep -q 'subgroup barrier pipeline compiled on' "$work/pipeline.log" || {
	cat "$work/pipeline.log" >&2
	echo "build-moltenvk: subgroup barrier Vulkan pipeline was not compiled" >&2
	exit 1
}

echo "Built x86-64 MoltenVK ICD: $stage/lib/libMoltenVK.dylib"
echo "ICD manifest: $stage/share/vulkan/icd.d/MoltenVK_icd.json"
echo "Use VK_DRIVER_FILES=$stage/share/vulkan/icd.d/MoltenVK_icd.json with the Vulkan loader."
