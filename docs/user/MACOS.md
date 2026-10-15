# macOS

Running a title on macOS (Apple silicon, Rosetta 2, MoltenVK) with the scripts in [tools/macos](../../tools/macos).

## Requirements

- Xcode or its command-line tools (`xcode-select --install`), and Rosetta 2 (`softwareupdate --install-rosetta`).
- CMake and Ninja, for example `brew install cmake ninja`, and the submodules: `git submodule update --init --recursive --depth 1`.
- The LunarG Vulkan SDK, whose loader and MoltenVK are universal binaries; Homebrew's are arm64 only and the libraries run as x86-64. Download `vulkan_sdk.zip` from [vulkan.lunarg.com](https://vulkan.lunarg.com/sdk/home#mac), unzip it and install the core component into your home directory without administrator rights:

  ```
  vulkansdk-macOS-<version>.app/Contents/MacOS/vulkansdk-macOS-<version> --root ~/VulkanSDK/<version> --accept-licenses --default-answer --confirm-command install com.lunarg.vulkan.core
  ```

  Check that MoltenVK sees the GPU under Rosetta (`arch` drops `DYLD_*` variables unless they are passed with `-e`):

  ```
  sdk=~/VulkanSDK/<version>/macOS
  arch -x86_64 -e DYLD_LIBRARY_PATH=$sdk/lib -e VK_DRIVER_FILES=$sdk/share/vulkan/icd.d/MoltenVK_icd.json $sdk/bin/vulkaninfo --summary
  ```

Checked on an Apple M4 (16 GB) with macOS 27.0.1, Apple clang 21, CMake 4, Ninja 1.13 and Vulkan SDK 1.4.363.0 (MoltenVK 1.4.2).

## Build

```
cmake -S . -B ../build-macos -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 -DBUILD_TESTING=ON
cmake --build ../build-macos -- -k 0
```

The scripts look for the build in `../build-macos`; set `ANYPS5_BUILD` for another directory. The Vulkan SDK is found in `~/VulkanSDK/<version>/macOS`; set `VULKAN_SDK` for another one.

## Tests

```
ctest --test-dir ../build-macos -E 'agc|flip|display|vulkan|gpu' -j4 --timeout 180
```

The GPU tests need the SDK's loader and MoltenVK; run them one at a time:

```
sdk=~/VulkanSDK/<version>/macOS
DYLD_LIBRARY_PATH=$sdk/lib VK_DRIVER_FILES=$sdk/share/vulkan/icd.d/MoltenVK_icd.json ctest --test-dir ../build-macos -R 'agc|vulkan|gpu' -E 'flip|display|lds_atomics64' -j1 --timeout 30
```

With more parallel jobs than cores, timing-sensitive tests fail at random; rerun them with `--rerun-failed` before reading anything into it.

## Set up a title

```
tools/macos/setup-game.sh <dump directory> <game directory> [module directory ...]
```

The dump directory holds `eboot.bin`, from a decrypted dump of a title you own. Module directories are the ones the title loads besides `sce_module`, relative to the dump, for example `Media/Modules Media/Plugins` for Unity titles. The script unwraps the SELF files into `<game directory>/.relink-input`, relinks them, links the rest of the dump into `app0/`, links `libs/` to the build and copies the launch scripts. It does not modify the dump.

Run it again after a change to the relinker. A change to the system libraries only needs the build.

## Run

- `<game directory>/run.sh`: plays the title. Background pipeline builds (`APS5_ASYNC_PIPELINES`) are on; `APS5_ASYNC_PIPELINES=0 ./run.sh` turns them off.
- `<game directory>/run-profile.sh`: the same with `APS5_PROFILE_DRAW` and `APS5_PROFILE_GPU`, logged to `logs/profile-<date>.log`.
- `python3 -I tools/macos/profile_summary.py <log> [--windows]`: frame rate and per-draw cost by scene weight, and the longest stalls.

Save data is in `<game directory>/_sd`.
