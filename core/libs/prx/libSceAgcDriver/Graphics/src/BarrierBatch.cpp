#include "prx/libSceAgcDriver/Graphics/include/BarrierBatch.hpp"

namespace AgcDriver::Graphics {

namespace {

bool StageCovers(VkPipelineStageFlags outer, VkPipelineStageFlags inner) {
    auto rest = inner & ~outer;
    if ((outer & VK_PIPELINE_STAGE_ALL_COMMANDS_BIT) != 0) rest &= VK_PIPELINE_STAGE_HOST_BIT;
    return rest == 0;
}

}

bool Covers(const BarrierMask& outer, const BarrierMask& inner) {
    if (inner.changesLayout || inner.transfersQueue) return false;
    return StageCovers(outer.srcStage, inner.srcStage) && StageCovers(outer.dstStage, inner.dstStage) && (inner.srcAccess & ~outer.srcAccess) == 0 && (inner.dstAccess & ~outer.dstAccess) == 0;
}

bool OrderedByTrailing(VkAccessFlags covered, const BarrierMask& wanted) {
    if (wanted.changesLayout || wanted.transfersQueue || (wanted.dstStage & VK_PIPELINE_STAGE_HOST_BIT) != 0) return false;
    constexpr VkAccessFlags shaderAndTransfer = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    const VkAccessFlags needed = shaderAndTransfer | wanted.dstAccess;
    return (covered & needed) == needed;
}

}
