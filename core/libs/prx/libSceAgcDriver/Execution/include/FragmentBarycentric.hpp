#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_FRAGMENTBARYCENTRIC_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_FRAGMENTBARYCENTRIC_HPP

#include <vulkan/vulkan.h>

namespace AgcDriver {

inline bool FragmentShaderBarycentricUsable(VkDriverId driver, bool reported) {
    return reported && driver != VK_DRIVER_ID_MOLTENVK;
}

}

#endif
