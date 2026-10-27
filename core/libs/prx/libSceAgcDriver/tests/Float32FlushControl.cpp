#include "prx/libSceAgcDriver/Execution/include/Float32FlushControl.hpp"
#include <cstdio>

namespace {

struct Case {
    VkDriverId driver;
    VkBool32 flushToZero;
    VkShaderFloatControlsIndependence independence;
    bool probe;
    bool declared;
};

constexpr Case Cases[]{
    {VK_DRIVER_ID_MOLTENVK, VK_FALSE, VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_NONE, true, false},
    {VK_DRIVER_ID_MOLTENVK, VK_TRUE, VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_ALL, true, true},
    {VK_DRIVER_ID_MESA_RADV, VK_TRUE, VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_ALL, true, true},
    {VK_DRIVER_ID_MESA_RADV, VK_TRUE, VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_32_BIT_ONLY, true, true},
    {VK_DRIVER_ID_MESA_RADV, VK_TRUE, VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_NONE, false, false},
    {VK_DRIVER_ID_NVIDIA_PROPRIETARY, VK_FALSE, VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_ALL, true, false},
    {VK_DRIVER_ID_NVIDIA_PROPRIETARY, VK_TRUE, VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_ALL, true, true},
    {VK_DRIVER_ID_AMD_PROPRIETARY, VK_TRUE, VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_32_BIT_ONLY, true, true},
    {static_cast<VkDriverId>(0), VK_FALSE, VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_NONE, false, false},
};

}

int main() {
    int failures = 0;
    for (const auto& item : Cases) {
        const auto control = AgcDriver::Float32FlushControlFor(item.driver, item.flushToZero, item.independence);
        if (control.probe != item.probe || control.declared != item.declared) {
            std::fprintf(stderr, "driver %d flush %u independence %d: expected probe %d declared %d, got %d %d\n", static_cast<int>(item.driver), static_cast<unsigned>(item.flushToZero), static_cast<int>(item.independence), item.probe ? 1 : 0, item.declared ? 1 : 0, control.probe ? 1 : 0, control.declared ? 1 : 0);
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}
