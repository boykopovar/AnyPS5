#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BARRIERBATCH_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BARRIERBATCH_HPP

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

namespace AgcDriver::Graphics {

struct BarrierMask {
    VkPipelineStageFlags srcStage = 0;
    VkPipelineStageFlags dstStage = 0;
    VkAccessFlags srcAccess = 0;
    VkAccessFlags dstAccess = 0;
    bool changesLayout = false;
    bool transfersQueue = false;
};

bool Covers(const BarrierMask& outer, const BarrierMask& inner);
bool OrderedByTrailing(VkAccessFlags covered, const BarrierMask& wanted);

}

#endif
