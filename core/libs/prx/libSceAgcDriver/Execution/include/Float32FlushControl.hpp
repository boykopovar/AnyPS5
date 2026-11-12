#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_FLOAT32FLUSHCONTROL_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_FLOAT32FLUSHCONTROL_HPP

#include <vulkan/vulkan.h>

namespace AgcDriver {

struct Float32FlushControl {
    bool probe = false;
    bool declared = false;
    bool computeOnly = false;
};

inline Float32FlushControl Float32FlushControlFor(VkDriverId driver, VkBool32 flushToZero, VkShaderFloatControlsIndependence independence) {
    const bool declared = flushToZero == VK_TRUE && independence != VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_NONE;
    const bool kosmicKrisp = driver == VK_DRIVER_ID_MESA_KOSMICKRISP;
    return {declared || driver == VK_DRIVER_ID_MOLTENVK || kosmicKrisp || driver == VK_DRIVER_ID_NVIDIA_PROPRIETARY, declared, kosmicKrisp};
}

}

#endif
