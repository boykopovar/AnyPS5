#include "prx/libSceAgcDriver/Graphics/include/CommandCheckpoints.hpp"
#include <atomic>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>

namespace AgcDriver::Graphics {

namespace {

struct RecordedCheckpoint {
    VkCommandBuffer commands = VK_NULL_HANDLE;
    AgcDriver::FaultToken token = 0;
    bool tokenIsLast = false;
};

RecordedCheckpoint& lastRecorded() noexcept {
    thread_local RecordedCheckpoint recorded;
    return recorded;
}

std::atomic<PFN_vkGetDeviceProcAddr>& resolverSlot() noexcept {
    static std::atomic<PFN_vkGetDeviceProcAddr> slot{nullptr};
    return slot;
}

std::atomic<PFN_vkCmdSetCheckpointNV>& setCheckpointSlot() noexcept {
    static std::atomic<PFN_vkCmdSetCheckpointNV> slot{nullptr};
    return slot;
}

void mark(VkCommandBuffer commands, AgcDriver::CheckpointKind kind) noexcept {
    const auto setCheckpoint = setCheckpointSlot().load(std::memory_order_acquire);
    if (setCheckpoint == nullptr || commands == VK_NULL_HANDLE) return;
    auto& recorded = lastRecorded();
    const AgcDriver::FaultToken token = recorded.commands == commands ? recorded.token : 0;
    if (recorded.commands == commands) recorded.tokenIsLast = false;
    setCheckpoint(commands, reinterpret_cast<const void*>(AgcDriver::CheckpointMarker(token, kind)));
}

void markWork(VkCommandBuffer commands, AgcDriver::CheckpointKind kind) noexcept {
    auto& recorded = lastRecorded();
    if (recorded.commands == commands && recorded.tokenIsLast) {
        recorded.tokenIsLast = false;
        return;
    }
    mark(commands, kind);
}

enum class Slot : std::size_t {
    BeginCommandBuffer,
    EndCommandBuffer,
    CmdDraw,
    CmdDrawIndexed,
    CmdDrawIndirect,
    CmdDrawIndexedIndirect,
    CmdDrawIndirectCount,
    CmdDrawIndexedIndirectCount,
    CmdDrawMeshTasks,
    CmdDrawMeshTasksIndirect,
    CmdDispatch,
    CmdDispatchBase,
    CmdDispatchIndirect,
    CmdCopyBuffer,
    CmdCopyImage,
    CmdCopyBufferToImage,
    CmdCopyImageToBuffer,
    CmdBlitImage,
    CmdResolveImage,
    CmdClearColorImage,
    CmdClearDepthStencilImage,
    CmdClearAttachments,
    CmdFillBuffer,
    CmdUpdateBuffer,
    CmdPipelineBarrier,
    CmdPipelineBarrier2,
    CmdBeginRenderPass,
    CmdEndRenderPass,
    CmdBeginRendering,
    CmdEndRendering,
    CmdResetQueryPool,
    CmdBeginQuery,
    CmdEndQuery,
    CmdCopyQueryPoolResults,
    CmdWriteTimestamp,
    CmdExecuteCommands,
    CmdSetEvent,
    CmdResetEvent,
    CmdWaitEvents,
    CmdSetEvent2,
    CmdResetEvent2,
    CmdWaitEvents2,
    CmdDrawIndirectCountCore,
    CmdDrawIndexedIndirectCountCore,
    CmdPipelineBarrier2KHR,
    CmdBeginRenderingKHR,
    CmdEndRenderingKHR,
    CmdSetEvent2KHR,
    CmdResetEvent2KHR,
    CmdWaitEvents2KHR,
};

template<Slot TCommand, typename TFunction>
struct Real {
    static std::atomic<TFunction>& Holder() noexcept {
        static std::atomic<TFunction> slot{nullptr};
        return slot;
    }
    static TFunction Get() noexcept {
        return Holder().load(std::memory_order_acquire);
    }
    static void Store(PFN_vkVoidFunction real, const char* name) {
        const auto function = reinterpret_cast<TFunction>(real);
        TFunction expected = Holder().load(std::memory_order_acquire);
        if (expected == nullptr && Holder().compare_exchange_strong(expected, function, std::memory_order_acq_rel)) return;
        if (expected == function) return;
        throw std::runtime_error(std::string("AGC graphics: command checkpoints got two different functions for ") + name);
    }
};

VKAPI_ATTR VkResult VKAPI_CALL checkpointedBeginCommandBuffer(VkCommandBuffer commands, const VkCommandBufferBeginInfo* info) {
    const auto result = Real<Slot::BeginCommandBuffer, PFN_vkBeginCommandBuffer>::Get()(commands, info);
    auto& recorded = lastRecorded();
    if (recorded.commands == commands) recorded.token = 0;
    if (result == VK_SUCCESS) mark(commands, AgcDriver::CheckpointKind::commandBufferBegin);
    return result;
}

VKAPI_ATTR VkResult VKAPI_CALL checkpointedEndCommandBuffer(VkCommandBuffer commands) {
    mark(commands, AgcDriver::CheckpointKind::commandBufferEnd);
    return Real<Slot::EndCommandBuffer, PFN_vkEndCommandBuffer>::Get()(commands);
}

VKAPI_ATTR void VKAPI_CALL checkpointedDraw(VkCommandBuffer commands, std::uint32_t vertexCount, std::uint32_t instanceCount, std::uint32_t firstVertex, std::uint32_t firstInstance) {
    markWork(commands, AgcDriver::CheckpointKind::draw);
    Real<Slot::CmdDraw, PFN_vkCmdDraw>::Get()(commands, vertexCount, instanceCount, firstVertex, firstInstance);
}

VKAPI_ATTR void VKAPI_CALL checkpointedDrawIndexed(VkCommandBuffer commands, std::uint32_t indexCount, std::uint32_t instanceCount, std::uint32_t firstIndex, std::int32_t vertexOffset, std::uint32_t firstInstance) {
    markWork(commands, AgcDriver::CheckpointKind::draw);
    Real<Slot::CmdDrawIndexed, PFN_vkCmdDrawIndexed>::Get()(commands, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

VKAPI_ATTR void VKAPI_CALL checkpointedDrawIndirect(VkCommandBuffer commands, VkBuffer buffer, VkDeviceSize offset, std::uint32_t drawCount, std::uint32_t stride) {
    markWork(commands, AgcDriver::CheckpointKind::draw);
    Real<Slot::CmdDrawIndirect, PFN_vkCmdDrawIndirect>::Get()(commands, buffer, offset, drawCount, stride);
}

VKAPI_ATTR void VKAPI_CALL checkpointedDrawIndexedIndirect(VkCommandBuffer commands, VkBuffer buffer, VkDeviceSize offset, std::uint32_t drawCount, std::uint32_t stride) {
    markWork(commands, AgcDriver::CheckpointKind::draw);
    Real<Slot::CmdDrawIndexedIndirect, PFN_vkCmdDrawIndexedIndirect>::Get()(commands, buffer, offset, drawCount, stride);
}

template<Slot TCommand>
VKAPI_ATTR void VKAPI_CALL checkpointedDrawIndirectCount(VkCommandBuffer commands, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countOffset, std::uint32_t maxDrawCount, std::uint32_t stride) {
    markWork(commands, AgcDriver::CheckpointKind::draw);
    Real<TCommand, PFN_vkCmdDrawIndirectCountKHR>::Get()(commands, buffer, offset, countBuffer, countOffset, maxDrawCount, stride);
}

template<Slot TCommand>
VKAPI_ATTR void VKAPI_CALL checkpointedDrawIndexedIndirectCount(VkCommandBuffer commands, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countOffset, std::uint32_t maxDrawCount, std::uint32_t stride) {
    markWork(commands, AgcDriver::CheckpointKind::draw);
    Real<TCommand, PFN_vkCmdDrawIndexedIndirectCountKHR>::Get()(commands, buffer, offset, countBuffer, countOffset, maxDrawCount, stride);
}

VKAPI_ATTR void VKAPI_CALL checkpointedDrawMeshTasks(VkCommandBuffer commands, std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    markWork(commands, AgcDriver::CheckpointKind::draw);
    Real<Slot::CmdDrawMeshTasks, PFN_vkCmdDrawMeshTasksEXT>::Get()(commands, x, y, z);
}

VKAPI_ATTR void VKAPI_CALL checkpointedDrawMeshTasksIndirect(VkCommandBuffer commands, VkBuffer buffer, VkDeviceSize offset, std::uint32_t drawCount, std::uint32_t stride) {
    markWork(commands, AgcDriver::CheckpointKind::draw);
    Real<Slot::CmdDrawMeshTasksIndirect, PFN_vkCmdDrawMeshTasksIndirectEXT>::Get()(commands, buffer, offset, drawCount, stride);
}

VKAPI_ATTR void VKAPI_CALL checkpointedDispatch(VkCommandBuffer commands, std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    markWork(commands, AgcDriver::CheckpointKind::dispatch);
    Real<Slot::CmdDispatch, PFN_vkCmdDispatch>::Get()(commands, x, y, z);
}

VKAPI_ATTR void VKAPI_CALL checkpointedDispatchBase(VkCommandBuffer commands, std::uint32_t baseX, std::uint32_t baseY, std::uint32_t baseZ, std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    markWork(commands, AgcDriver::CheckpointKind::dispatch);
    Real<Slot::CmdDispatchBase, PFN_vkCmdDispatchBase>::Get()(commands, baseX, baseY, baseZ, x, y, z);
}

VKAPI_ATTR void VKAPI_CALL checkpointedDispatchIndirect(VkCommandBuffer commands, VkBuffer buffer, VkDeviceSize offset) {
    markWork(commands, AgcDriver::CheckpointKind::dispatch);
    Real<Slot::CmdDispatchIndirect, PFN_vkCmdDispatchIndirect>::Get()(commands, buffer, offset);
}

VKAPI_ATTR void VKAPI_CALL checkpointedCopyBuffer(VkCommandBuffer commands, VkBuffer source, VkBuffer destination, std::uint32_t regionCount, const VkBufferCopy* regions) {
    mark(commands, AgcDriver::CheckpointKind::copy);
    Real<Slot::CmdCopyBuffer, PFN_vkCmdCopyBuffer>::Get()(commands, source, destination, regionCount, regions);
}

VKAPI_ATTR void VKAPI_CALL checkpointedCopyImage(VkCommandBuffer commands, VkImage source, VkImageLayout sourceLayout, VkImage destination, VkImageLayout destinationLayout, std::uint32_t regionCount, const VkImageCopy* regions) {
    mark(commands, AgcDriver::CheckpointKind::copy);
    Real<Slot::CmdCopyImage, PFN_vkCmdCopyImage>::Get()(commands, source, sourceLayout, destination, destinationLayout, regionCount, regions);
}

VKAPI_ATTR void VKAPI_CALL checkpointedCopyBufferToImage(VkCommandBuffer commands, VkBuffer source, VkImage destination, VkImageLayout layout, std::uint32_t regionCount, const VkBufferImageCopy* regions) {
    mark(commands, AgcDriver::CheckpointKind::copy);
    Real<Slot::CmdCopyBufferToImage, PFN_vkCmdCopyBufferToImage>::Get()(commands, source, destination, layout, regionCount, regions);
}

VKAPI_ATTR void VKAPI_CALL checkpointedCopyImageToBuffer(VkCommandBuffer commands, VkImage source, VkImageLayout layout, VkBuffer destination, std::uint32_t regionCount, const VkBufferImageCopy* regions) {
    mark(commands, AgcDriver::CheckpointKind::copy);
    Real<Slot::CmdCopyImageToBuffer, PFN_vkCmdCopyImageToBuffer>::Get()(commands, source, layout, destination, regionCount, regions);
}

VKAPI_ATTR void VKAPI_CALL checkpointedBlitImage(VkCommandBuffer commands, VkImage source, VkImageLayout sourceLayout, VkImage destination, VkImageLayout destinationLayout, std::uint32_t regionCount, const VkImageBlit* regions, VkFilter filter) {
    mark(commands, AgcDriver::CheckpointKind::blit);
    Real<Slot::CmdBlitImage, PFN_vkCmdBlitImage>::Get()(commands, source, sourceLayout, destination, destinationLayout, regionCount, regions, filter);
}

VKAPI_ATTR void VKAPI_CALL checkpointedResolveImage(VkCommandBuffer commands, VkImage source, VkImageLayout sourceLayout, VkImage destination, VkImageLayout destinationLayout, std::uint32_t regionCount, const VkImageResolve* regions) {
    mark(commands, AgcDriver::CheckpointKind::blit);
    Real<Slot::CmdResolveImage, PFN_vkCmdResolveImage>::Get()(commands, source, sourceLayout, destination, destinationLayout, regionCount, regions);
}

VKAPI_ATTR void VKAPI_CALL checkpointedClearColorImage(VkCommandBuffer commands, VkImage image, VkImageLayout layout, const VkClearColorValue* color, std::uint32_t rangeCount, const VkImageSubresourceRange* ranges) {
    mark(commands, AgcDriver::CheckpointKind::clear);
    Real<Slot::CmdClearColorImage, PFN_vkCmdClearColorImage>::Get()(commands, image, layout, color, rangeCount, ranges);
}

VKAPI_ATTR void VKAPI_CALL checkpointedClearDepthStencilImage(VkCommandBuffer commands, VkImage image, VkImageLayout layout, const VkClearDepthStencilValue* value, std::uint32_t rangeCount, const VkImageSubresourceRange* ranges) {
    mark(commands, AgcDriver::CheckpointKind::clear);
    Real<Slot::CmdClearDepthStencilImage, PFN_vkCmdClearDepthStencilImage>::Get()(commands, image, layout, value, rangeCount, ranges);
}

VKAPI_ATTR void VKAPI_CALL checkpointedClearAttachments(VkCommandBuffer commands, std::uint32_t attachmentCount, const VkClearAttachment* attachments, std::uint32_t rectCount, const VkClearRect* rects) {
    mark(commands, AgcDriver::CheckpointKind::clear);
    Real<Slot::CmdClearAttachments, PFN_vkCmdClearAttachments>::Get()(commands, attachmentCount, attachments, rectCount, rects);
}

VKAPI_ATTR void VKAPI_CALL checkpointedFillBuffer(VkCommandBuffer commands, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, std::uint32_t data) {
    mark(commands, AgcDriver::CheckpointKind::fill);
    Real<Slot::CmdFillBuffer, PFN_vkCmdFillBuffer>::Get()(commands, buffer, offset, size, data);
}

VKAPI_ATTR void VKAPI_CALL checkpointedUpdateBuffer(VkCommandBuffer commands, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, const void* data) {
    mark(commands, AgcDriver::CheckpointKind::update);
    Real<Slot::CmdUpdateBuffer, PFN_vkCmdUpdateBuffer>::Get()(commands, buffer, offset, size, data);
}

VKAPI_ATTR void VKAPI_CALL checkpointedPipelineBarrier(VkCommandBuffer commands, VkPipelineStageFlags source, VkPipelineStageFlags destination, VkDependencyFlags dependency, std::uint32_t memoryCount, const VkMemoryBarrier* memory, std::uint32_t bufferCount, const VkBufferMemoryBarrier* buffers, std::uint32_t imageCount, const VkImageMemoryBarrier* images) {
    mark(commands, AgcDriver::CheckpointKind::barrier);
    Real<Slot::CmdPipelineBarrier, PFN_vkCmdPipelineBarrier>::Get()(commands, source, destination, dependency, memoryCount, memory, bufferCount, buffers, imageCount, images);
}

template<Slot TCommand>
VKAPI_ATTR void VKAPI_CALL checkpointedPipelineBarrier2(VkCommandBuffer commands, const VkDependencyInfo* dependency) {
    mark(commands, AgcDriver::CheckpointKind::barrier);
    Real<TCommand, PFN_vkCmdPipelineBarrier2>::Get()(commands, dependency);
}

VKAPI_ATTR void VKAPI_CALL checkpointedBeginRenderPass(VkCommandBuffer commands, const VkRenderPassBeginInfo* begin, VkSubpassContents contents) {
    mark(commands, AgcDriver::CheckpointKind::renderPassBegin);
    Real<Slot::CmdBeginRenderPass, PFN_vkCmdBeginRenderPass>::Get()(commands, begin, contents);
}

VKAPI_ATTR void VKAPI_CALL checkpointedEndRenderPass(VkCommandBuffer commands) {
    mark(commands, AgcDriver::CheckpointKind::renderPassEnd);
    Real<Slot::CmdEndRenderPass, PFN_vkCmdEndRenderPass>::Get()(commands);
}

template<Slot TCommand>
VKAPI_ATTR void VKAPI_CALL checkpointedBeginRendering(VkCommandBuffer commands, const VkRenderingInfo* info) {
    mark(commands, AgcDriver::CheckpointKind::renderPassBegin);
    Real<TCommand, PFN_vkCmdBeginRendering>::Get()(commands, info);
}

template<Slot TCommand>
VKAPI_ATTR void VKAPI_CALL checkpointedEndRendering(VkCommandBuffer commands) {
    mark(commands, AgcDriver::CheckpointKind::renderPassEnd);
    Real<TCommand, PFN_vkCmdEndRendering>::Get()(commands);
}

VKAPI_ATTR void VKAPI_CALL checkpointedResetQueryPool(VkCommandBuffer commands, VkQueryPool pool, std::uint32_t first, std::uint32_t count) {
    mark(commands, AgcDriver::CheckpointKind::query);
    Real<Slot::CmdResetQueryPool, PFN_vkCmdResetQueryPool>::Get()(commands, pool, first, count);
}

VKAPI_ATTR void VKAPI_CALL checkpointedBeginQuery(VkCommandBuffer commands, VkQueryPool pool, std::uint32_t query, VkQueryControlFlags flags) {
    mark(commands, AgcDriver::CheckpointKind::query);
    Real<Slot::CmdBeginQuery, PFN_vkCmdBeginQuery>::Get()(commands, pool, query, flags);
}

VKAPI_ATTR void VKAPI_CALL checkpointedEndQuery(VkCommandBuffer commands, VkQueryPool pool, std::uint32_t query) {
    mark(commands, AgcDriver::CheckpointKind::query);
    Real<Slot::CmdEndQuery, PFN_vkCmdEndQuery>::Get()(commands, pool, query);
}

VKAPI_ATTR void VKAPI_CALL checkpointedCopyQueryPoolResults(VkCommandBuffer commands, VkQueryPool pool, std::uint32_t first, std::uint32_t count, VkBuffer destination, VkDeviceSize offset, VkDeviceSize stride, VkQueryResultFlags flags) {
    mark(commands, AgcDriver::CheckpointKind::query);
    Real<Slot::CmdCopyQueryPoolResults, PFN_vkCmdCopyQueryPoolResults>::Get()(commands, pool, first, count, destination, offset, stride, flags);
}

VKAPI_ATTR void VKAPI_CALL checkpointedWriteTimestamp(VkCommandBuffer commands, VkPipelineStageFlagBits stage, VkQueryPool pool, std::uint32_t query) {
    mark(commands, AgcDriver::CheckpointKind::query);
    Real<Slot::CmdWriteTimestamp, PFN_vkCmdWriteTimestamp>::Get()(commands, stage, pool, query);
}

VKAPI_ATTR void VKAPI_CALL checkpointedExecuteCommands(VkCommandBuffer commands, std::uint32_t count, const VkCommandBuffer* buffers) {
    mark(commands, AgcDriver::CheckpointKind::execute);
    Real<Slot::CmdExecuteCommands, PFN_vkCmdExecuteCommands>::Get()(commands, count, buffers);
}

VKAPI_ATTR void VKAPI_CALL checkpointedSetEvent(VkCommandBuffer commands, VkEvent event, VkPipelineStageFlags stage) {
    mark(commands, AgcDriver::CheckpointKind::event);
    Real<Slot::CmdSetEvent, PFN_vkCmdSetEvent>::Get()(commands, event, stage);
}

VKAPI_ATTR void VKAPI_CALL checkpointedResetEvent(VkCommandBuffer commands, VkEvent event, VkPipelineStageFlags stage) {
    mark(commands, AgcDriver::CheckpointKind::event);
    Real<Slot::CmdResetEvent, PFN_vkCmdResetEvent>::Get()(commands, event, stage);
}

VKAPI_ATTR void VKAPI_CALL checkpointedWaitEvents(VkCommandBuffer commands, std::uint32_t eventCount, const VkEvent* events, VkPipelineStageFlags source, VkPipelineStageFlags destination, std::uint32_t memoryCount, const VkMemoryBarrier* memory, std::uint32_t bufferCount, const VkBufferMemoryBarrier* buffers, std::uint32_t imageCount, const VkImageMemoryBarrier* images) {
    mark(commands, AgcDriver::CheckpointKind::wait);
    Real<Slot::CmdWaitEvents, PFN_vkCmdWaitEvents>::Get()(commands, eventCount, events, source, destination, memoryCount, memory, bufferCount, buffers, imageCount, images);
}

template<Slot TCommand>
VKAPI_ATTR void VKAPI_CALL checkpointedSetEvent2(VkCommandBuffer commands, VkEvent event, const VkDependencyInfo* dependency) {
    mark(commands, AgcDriver::CheckpointKind::event);
    Real<TCommand, PFN_vkCmdSetEvent2>::Get()(commands, event, dependency);
}

template<Slot TCommand>
VKAPI_ATTR void VKAPI_CALL checkpointedResetEvent2(VkCommandBuffer commands, VkEvent event, VkPipelineStageFlags2 stage) {
    mark(commands, AgcDriver::CheckpointKind::event);
    Real<TCommand, PFN_vkCmdResetEvent2>::Get()(commands, event, stage);
}

template<Slot TCommand>
VKAPI_ATTR void VKAPI_CALL checkpointedWaitEvents2(VkCommandBuffer commands, std::uint32_t eventCount, const VkEvent* events, const VkDependencyInfo* dependencies) {
    mark(commands, AgcDriver::CheckpointKind::wait);
    Real<TCommand, PFN_vkCmdWaitEvents2>::Get()(commands, eventCount, events, dependencies);
}

struct Hook {
    const char* name;
    void (*store)(PFN_vkVoidFunction real, const char* name);
    PFN_vkVoidFunction wrapper;
};

template<Slot TCommand, typename TFunction>
Hook hook(const char* name, TFunction wrapper) noexcept {
    return {name, &Real<TCommand, TFunction>::Store, reinterpret_cast<PFN_vkVoidFunction>(wrapper)};
}

const Hook* findHook(const char* name) noexcept {
    static const Hook hooks[] = {
        hook<Slot::BeginCommandBuffer, PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer", &checkpointedBeginCommandBuffer),
        hook<Slot::EndCommandBuffer, PFN_vkEndCommandBuffer>("vkEndCommandBuffer", &checkpointedEndCommandBuffer),
        hook<Slot::CmdDraw, PFN_vkCmdDraw>("vkCmdDraw", &checkpointedDraw),
        hook<Slot::CmdDrawIndexed, PFN_vkCmdDrawIndexed>("vkCmdDrawIndexed", &checkpointedDrawIndexed),
        hook<Slot::CmdDrawIndirect, PFN_vkCmdDrawIndirect>("vkCmdDrawIndirect", &checkpointedDrawIndirect),
        hook<Slot::CmdDrawIndexedIndirect, PFN_vkCmdDrawIndexedIndirect>("vkCmdDrawIndexedIndirect", &checkpointedDrawIndexedIndirect),
        hook<Slot::CmdDrawIndirectCount, PFN_vkCmdDrawIndirectCountKHR>("vkCmdDrawIndirectCountKHR", &checkpointedDrawIndirectCount<Slot::CmdDrawIndirectCount>),
        hook<Slot::CmdDrawIndirectCountCore, PFN_vkCmdDrawIndirectCountKHR>("vkCmdDrawIndirectCount", &checkpointedDrawIndirectCount<Slot::CmdDrawIndirectCountCore>),
        hook<Slot::CmdDrawIndexedIndirectCount, PFN_vkCmdDrawIndexedIndirectCountKHR>("vkCmdDrawIndexedIndirectCountKHR", &checkpointedDrawIndexedIndirectCount<Slot::CmdDrawIndexedIndirectCount>),
        hook<Slot::CmdDrawIndexedIndirectCountCore, PFN_vkCmdDrawIndexedIndirectCountKHR>("vkCmdDrawIndexedIndirectCount", &checkpointedDrawIndexedIndirectCount<Slot::CmdDrawIndexedIndirectCountCore>),
        hook<Slot::CmdDrawMeshTasks, PFN_vkCmdDrawMeshTasksEXT>("vkCmdDrawMeshTasksEXT", &checkpointedDrawMeshTasks),
        hook<Slot::CmdDrawMeshTasksIndirect, PFN_vkCmdDrawMeshTasksIndirectEXT>("vkCmdDrawMeshTasksIndirectEXT", &checkpointedDrawMeshTasksIndirect),
        hook<Slot::CmdDispatch, PFN_vkCmdDispatch>("vkCmdDispatch", &checkpointedDispatch),
        hook<Slot::CmdDispatchBase, PFN_vkCmdDispatchBase>("vkCmdDispatchBase", &checkpointedDispatchBase),
        hook<Slot::CmdDispatchIndirect, PFN_vkCmdDispatchIndirect>("vkCmdDispatchIndirect", &checkpointedDispatchIndirect),
        hook<Slot::CmdCopyBuffer, PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer", &checkpointedCopyBuffer),
        hook<Slot::CmdCopyImage, PFN_vkCmdCopyImage>("vkCmdCopyImage", &checkpointedCopyImage),
        hook<Slot::CmdCopyBufferToImage, PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage", &checkpointedCopyBufferToImage),
        hook<Slot::CmdCopyImageToBuffer, PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer", &checkpointedCopyImageToBuffer),
        hook<Slot::CmdBlitImage, PFN_vkCmdBlitImage>("vkCmdBlitImage", &checkpointedBlitImage),
        hook<Slot::CmdResolveImage, PFN_vkCmdResolveImage>("vkCmdResolveImage", &checkpointedResolveImage),
        hook<Slot::CmdClearColorImage, PFN_vkCmdClearColorImage>("vkCmdClearColorImage", &checkpointedClearColorImage),
        hook<Slot::CmdClearDepthStencilImage, PFN_vkCmdClearDepthStencilImage>("vkCmdClearDepthStencilImage", &checkpointedClearDepthStencilImage),
        hook<Slot::CmdClearAttachments, PFN_vkCmdClearAttachments>("vkCmdClearAttachments", &checkpointedClearAttachments),
        hook<Slot::CmdFillBuffer, PFN_vkCmdFillBuffer>("vkCmdFillBuffer", &checkpointedFillBuffer),
        hook<Slot::CmdUpdateBuffer, PFN_vkCmdUpdateBuffer>("vkCmdUpdateBuffer", &checkpointedUpdateBuffer),
        hook<Slot::CmdPipelineBarrier, PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier", &checkpointedPipelineBarrier),
        hook<Slot::CmdPipelineBarrier2, PFN_vkCmdPipelineBarrier2>("vkCmdPipelineBarrier2", &checkpointedPipelineBarrier2<Slot::CmdPipelineBarrier2>),
        hook<Slot::CmdPipelineBarrier2KHR, PFN_vkCmdPipelineBarrier2>("vkCmdPipelineBarrier2KHR", &checkpointedPipelineBarrier2<Slot::CmdPipelineBarrier2KHR>),
        hook<Slot::CmdBeginRenderPass, PFN_vkCmdBeginRenderPass>("vkCmdBeginRenderPass", &checkpointedBeginRenderPass),
        hook<Slot::CmdEndRenderPass, PFN_vkCmdEndRenderPass>("vkCmdEndRenderPass", &checkpointedEndRenderPass),
        hook<Slot::CmdBeginRendering, PFN_vkCmdBeginRendering>("vkCmdBeginRendering", &checkpointedBeginRendering<Slot::CmdBeginRendering>),
        hook<Slot::CmdBeginRenderingKHR, PFN_vkCmdBeginRendering>("vkCmdBeginRenderingKHR", &checkpointedBeginRendering<Slot::CmdBeginRenderingKHR>),
        hook<Slot::CmdEndRendering, PFN_vkCmdEndRendering>("vkCmdEndRendering", &checkpointedEndRendering<Slot::CmdEndRendering>),
        hook<Slot::CmdEndRenderingKHR, PFN_vkCmdEndRendering>("vkCmdEndRenderingKHR", &checkpointedEndRendering<Slot::CmdEndRenderingKHR>),
        hook<Slot::CmdResetQueryPool, PFN_vkCmdResetQueryPool>("vkCmdResetQueryPool", &checkpointedResetQueryPool),
        hook<Slot::CmdBeginQuery, PFN_vkCmdBeginQuery>("vkCmdBeginQuery", &checkpointedBeginQuery),
        hook<Slot::CmdEndQuery, PFN_vkCmdEndQuery>("vkCmdEndQuery", &checkpointedEndQuery),
        hook<Slot::CmdCopyQueryPoolResults, PFN_vkCmdCopyQueryPoolResults>("vkCmdCopyQueryPoolResults", &checkpointedCopyQueryPoolResults),
        hook<Slot::CmdWriteTimestamp, PFN_vkCmdWriteTimestamp>("vkCmdWriteTimestamp", &checkpointedWriteTimestamp),
        hook<Slot::CmdExecuteCommands, PFN_vkCmdExecuteCommands>("vkCmdExecuteCommands", &checkpointedExecuteCommands),
        hook<Slot::CmdSetEvent, PFN_vkCmdSetEvent>("vkCmdSetEvent", &checkpointedSetEvent),
        hook<Slot::CmdResetEvent, PFN_vkCmdResetEvent>("vkCmdResetEvent", &checkpointedResetEvent),
        hook<Slot::CmdWaitEvents, PFN_vkCmdWaitEvents>("vkCmdWaitEvents", &checkpointedWaitEvents),
        hook<Slot::CmdSetEvent2, PFN_vkCmdSetEvent2>("vkCmdSetEvent2", &checkpointedSetEvent2<Slot::CmdSetEvent2>),
        hook<Slot::CmdSetEvent2KHR, PFN_vkCmdSetEvent2>("vkCmdSetEvent2KHR", &checkpointedSetEvent2<Slot::CmdSetEvent2KHR>),
        hook<Slot::CmdResetEvent2, PFN_vkCmdResetEvent2>("vkCmdResetEvent2", &checkpointedResetEvent2<Slot::CmdResetEvent2>),
        hook<Slot::CmdResetEvent2KHR, PFN_vkCmdResetEvent2>("vkCmdResetEvent2KHR", &checkpointedResetEvent2<Slot::CmdResetEvent2KHR>),
        hook<Slot::CmdWaitEvents2, PFN_vkCmdWaitEvents2>("vkCmdWaitEvents2", &checkpointedWaitEvents2<Slot::CmdWaitEvents2>),
        hook<Slot::CmdWaitEvents2KHR, PFN_vkCmdWaitEvents2>("vkCmdWaitEvents2KHR", &checkpointedWaitEvents2<Slot::CmdWaitEvents2KHR>),
    };
    for (const auto& entry : hooks) {
        if (std::strcmp(name, entry.name) == 0) return &entry;
    }
    return nullptr;
}

bool recordsState(const char* name) noexcept {
    static const char* const prefixes[] = {"vkCmdBind", "vkCmdSet", "vkCmdPush"};
    for (const auto* prefix : prefixes) {
        if (std::strncmp(name, prefix, std::strlen(prefix)) == 0) return true;
    }
    return std::strcmp(name, "vkCmdWriteBufferMarkerAMD") == 0 || std::strcmp(name, "vkCmdWriteBufferMarker2AMD") == 0;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL checkpointingDeviceProc(VkDevice device, const char* name) {
    const auto resolver = resolverSlot().load(std::memory_order_acquire);
    if (resolver == nullptr || name == nullptr) return nullptr;
    const auto real = resolver(device, name);
    if (real == nullptr) return nullptr;
    if (const auto* found = findHook(name)) {
        found->store(real, name);
        return found->wrapper;
    }
    if (std::strncmp(name, "vkCmd", 5) == 0 && !recordsState(name)) throw std::runtime_error(std::string("AGC graphics: command checkpoints do not know whether ") + name + " records work; add it to CommandCheckpoints.cpp");
    return real;
}

}

PFN_vkGetDeviceProcAddr CheckpointCommandKinds(VkDevice device, PFN_vkGetDeviceProcAddr resolver) {
    if (device == VK_NULL_HANDLE || resolver == nullptr) throw std::invalid_argument("AGC graphics: command checkpoints need a device and its resolver");
    const auto setCheckpoint = reinterpret_cast<PFN_vkCmdSetCheckpointNV>(resolver(device, "vkCmdSetCheckpointNV"));
    if (setCheckpoint == nullptr) throw std::runtime_error("AGC graphics: missing Vulkan function: vkCmdSetCheckpointNV");
    PFN_vkGetDeviceProcAddr expectedResolver = nullptr;
    if (!resolverSlot().compare_exchange_strong(expectedResolver, resolver, std::memory_order_acq_rel) && expectedResolver != resolver) throw std::runtime_error("AGC graphics: command checkpoints found two different device resolvers");
    PFN_vkCmdSetCheckpointNV expectedCheckpoint = nullptr;
    if (!setCheckpointSlot().compare_exchange_strong(expectedCheckpoint, setCheckpoint, std::memory_order_acq_rel) && expectedCheckpoint != setCheckpoint) throw std::runtime_error("AGC graphics: command checkpoints found two different vkCmdSetCheckpointNV functions");
    return &checkpointingDeviceProc;
}

void NoteRecordedCheckpoint(VkCommandBuffer commands, AgcDriver::FaultToken token) noexcept {
    auto& recorded = lastRecorded();
    recorded.commands = commands;
    recorded.token = token;
    recorded.tokenIsLast = true;
}

}
