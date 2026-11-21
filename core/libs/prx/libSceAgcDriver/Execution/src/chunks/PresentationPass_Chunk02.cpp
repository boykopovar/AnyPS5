#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Execution/include/PresentationPass.hpp"
#include "prx/libSceAgcDriver/Execution/include/AspectFit.hpp"
#include "prx/libSceAgcDriver/Execution/include/DisplayFormat.hpp"
#include "prx/libSceAgcDriver/Execution/include/PqToSdr.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/PresentationPassFrag_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/PresentationPassVert_spv.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstring>

namespace AgcDriver {
namespace {
void PresentationPass::RecordDump(VkCommandBuffer commands, std::size_t index, std::uint32_t stride, VkBuffer destination) {
    RecordDumpImage(commands, index, stride);
    RecordDumpReadback(commands, index, destination);
}

void PresentationPass::RecordDumpImage(VkCommandBuffer commands, std::size_t index, std::uint32_t stride) {
    Graphics::Require(index < slots.size() && slots[index].source != VK_NULL_HANDLE, "presentation pass slot has no source");
    auto& slot = slots[index];
    const auto extent = DumpExtent(slot.width, slot.height, stride);
    ensureDumpTarget(slot, extent);
    const auto& entry = target(VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    const std::int32_t parameters[9] = {0, 0, static_cast<std::int32_t>(extent.width), static_cast<std::int32_t>(extent.height), static_cast<std::int32_t>(slot.width), static_cast<std::int32_t>(slot.height), static_cast<std::int32_t>(stride), static_cast<std::int32_t>(slot.mode | ModeSubsample), std::bit_cast<std::int32_t>(slot.whiteNits)};
    draw(commands, slot, entry, slot.dumpFramebuffer, extent, {{0, 0}, extent}, parameters);
}

VkImage PresentationPass::DumpImage(std::size_t index) const {
    Graphics::Require(index < slots.size() && slots[index].dumpImage != VK_NULL_HANDLE, "presentation pass slot has no dump image");
    return slots[index].dumpImage;
}

VkExtent2D PresentationPass::DumpImageExtent(std::size_t index) const {
    Graphics::Require(index < slots.size() && slots[index].dumpImage != VK_NULL_HANDLE, "presentation pass slot has no dump image");
    return slots[index].dumpExtent;
}

void PresentationPass::RecordDumpReadback(VkCommandBuffer commands, std::size_t index, VkBuffer destination) {
    Graphics::Require(destination != VK_NULL_HANDLE, "presentation dump buffer is unavailable");
    const auto image = DumpImage(index);
    const auto extent = DumpImageExtent(index);
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {extent.width, extent.height, 1u};
    context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destination, 1, &copy);
    Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
}

}
}
