#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_COMMANDLIST_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_COMMANDLIST_HPP

#include <vulkan/vulkan.h>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#define APS5_COMMANDS(X) \
    X(CmdBeginQuery) \
    X(CmdBeginRenderPass) \
    X(CmdBeginRenderingKHR) \
    X(CmdBindDescriptorSets) \
    X(CmdBindIndexBuffer) \
    X(CmdBindPipeline) \
    X(CmdBindShadersEXT) \
    X(CmdBindVertexBuffers) \
    X(CmdBlitImage) \
    X(CmdClearAttachments) \
    X(CmdClearColorImage) \
    X(CmdClearDepthStencilImage) \
    X(CmdCopyBuffer) \
    X(CmdCopyBufferToImage) \
    X(CmdCopyImage) \
    X(CmdCopyImageToBuffer) \
    X(CmdCopyQueryPoolResults) \
    X(CmdDispatch) \
    X(CmdDispatchIndirect) \
    X(CmdDraw) \
    X(CmdDrawIndexed) \
    X(CmdDrawIndexedIndirect) \
    X(CmdDrawIndexedIndirectCountKHR) \
    X(CmdDrawIndirect) \
    X(CmdDrawIndirectCountKHR) \
    X(CmdDrawMeshTasksEXT) \
    X(CmdDrawMeshTasksIndirectEXT) \
    X(CmdEndQuery) \
    X(CmdEndRenderPass) \
    X(CmdEndRenderingKHR) \
    X(CmdExecuteCommands) \
    X(CmdFillBuffer) \
    X(CmdPipelineBarrier) \
    X(CmdPushConstants) \
    X(CmdPushDescriptorSetKHR) \
    X(CmdResetQueryPool) \
    X(CmdSetCheckpointNV) \
    X(CmdSetColorWriteMaskEXT) \
    X(CmdSetCullMode) \
    X(CmdSetCullModeEXT) \
    X(CmdSetDepthBias) \
    X(CmdSetDepthBias2EXT) \
    X(CmdSetDepthBounds) \
    X(CmdSetDepthBoundsTestEnable) \
    X(CmdSetDepthBoundsTestEnableEXT) \
    X(CmdSetDepthCompareOp) \
    X(CmdSetDepthCompareOpEXT) \
    X(CmdSetDepthTestEnable) \
    X(CmdSetDepthTestEnableEXT) \
    X(CmdSetDepthWriteEnable) \
    X(CmdSetDepthWriteEnableEXT) \
    X(CmdSetFrontFace) \
    X(CmdSetFrontFaceEXT) \
    X(CmdSetScissor) \
    X(CmdSetScissorWithCount) \
    X(CmdSetScissorWithCountEXT) \
    X(CmdSetStencilCompareMask) \
    X(CmdSetStencilOp) \
    X(CmdSetStencilOpEXT) \
    X(CmdSetStencilReference) \
    X(CmdSetStencilTestEnable) \
    X(CmdSetStencilTestEnableEXT) \
    X(CmdSetStencilWriteMask) \
    X(CmdSetViewport) \
    X(CmdSetViewportWithCount) \
    X(CmdSetViewportWithCountEXT) \
    X(CmdUpdateBuffer) \
    X(CmdWriteBufferMarkerAMD) \
    X(CmdWriteTimestamp)

namespace AgcDriver::Graphics {

enum class CommandId : std::uint16_t {
#define APS5_COMMAND_ID(name) name,
    APS5_COMMANDS(APS5_COMMAND_ID)
#undef APS5_COMMAND_ID
        Count
};

inline constexpr std::size_t CommandCount = static_cast<std::size_t>(CommandId::Count);

struct CommandFunctions {
#define APS5_COMMAND_MEMBER(name) PFN_vk##name name = nullptr;
    APS5_COMMANDS(APS5_COMMAND_MEMBER)
#undef APS5_COMMAND_MEMBER
};

std::span<const std::string_view> CommandNames();
bool IsCommandName(std::string_view name) noexcept;
CommandId CommandFor(std::string_view name);
void ResolveCommandFunctions(CommandFunctions& functions, VkDevice device, PFN_vkGetDeviceProcAddr resolve);
const CommandFunctions& StreamedCommands();
PFN_vkVoidFunction CommandThunk(std::string_view name, PFN_vkVoidFunction real, VkDevice device = VK_NULL_HANDLE);
void ForgetCommandDevice(VkDevice device);
std::size_t CommandDeviceEntries(CommandId id) noexcept;
bool IsCommandThunk(std::string_view name, PFN_vkVoidFunction function) noexcept;
std::size_t CommandThunkSlots(CommandId id) noexcept;
bool CommandStreams(CommandId id) noexcept;

}

#endif
