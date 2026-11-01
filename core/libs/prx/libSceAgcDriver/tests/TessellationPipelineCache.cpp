#include "prx/libSceAgcDriver/Execution/include/TessellationPipelineCache.hpp"
#include <cstdio>

namespace {

struct Case {
    VkDriverId driver;
    bool usable;
};

constexpr Case Cases[]{
    {VK_DRIVER_ID_MOLTENVK, false},
    {VK_DRIVER_ID_MESA_RADV, true},
    {VK_DRIVER_ID_MESA_LLVMPIPE, true},
    {VK_DRIVER_ID_NVIDIA_PROPRIETARY, true},
    {VK_DRIVER_ID_AMD_PROPRIETARY, true},
    {VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS, true},
};

}

int main() {
    int failures = 0;
    for (const auto& item : Cases) {
        if (AgcDriver::TessellationPipelineCacheUsable(item.driver) != item.usable) {
            std::fprintf(stderr, "driver %d: expected tessellation pipelines %s the pipeline cache\n", static_cast<int>(item.driver), item.usable ? "to use" : "not to use");
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}
