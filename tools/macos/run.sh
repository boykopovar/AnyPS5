#!/bin/sh
cd "$(dirname "$0")" || exit 1
sdk=${VULKAN_SDK:-$(ls -d "$HOME"/VulkanSDK/*/macOS 2>/dev/null | sort | tail -1)}
[ -n "$sdk" ] && [ -d "$sdk/lib" ] || { echo "no Vulkan SDK found: set VULKAN_SDK to its macOS directory" >&2; exit 1; }
export DYLD_LIBRARY_PATH="$sdk/lib"
export VK_DRIVER_FILES="$sdk/share/vulkan/icd.d/MoltenVK_icd.json"
export ROSETTA_ADVERTISE_AVX=1
export MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS=0
export MVK_CONFIG_SHOULD_MAXIMIZE_CONCURRENT_COMPILATION=1
if [ "${APS5_ASYNC_PIPELINES-1}" = 0 ]; then unset APS5_ASYNC_PIPELINES; else export APS5_ASYNC_PIPELINES=1; fi
exec ./eboot 2>&1 | grep -v '^objc\['
