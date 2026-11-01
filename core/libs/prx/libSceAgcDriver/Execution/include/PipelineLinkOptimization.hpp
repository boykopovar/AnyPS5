#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PIPELINELINKOPTIMIZATION_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PIPELINELINKOPTIMIZATION_HPP

#include <vulkan/vulkan.h>

namespace AgcDriver {

inline bool PipelineLinkTimeOptimizationUsable(VkDriverId driver) {
    return driver != VK_DRIVER_ID_NVIDIA_PROPRIETARY;
}

}

#endif
