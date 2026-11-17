#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_INDEXEDINDIRECTPATCHES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_INDEXEDINDIRECTPATCHES_HPP

#include <vulkan/vulkan.h>

namespace AgcDriver {

inline bool IndexedIndirectPatchesUsable(VkDriverId driver) {
    return driver != VK_DRIVER_ID_MOLTENVK;
}

}

#endif
