#include "prx/libSceAgcDriver/Graphics/include/CommandCheckpoints.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace AgcDriver::Graphics {
void WriteNvCheckpointToken(const Context& context, VkCommandBuffer commands, std::uint64_t token, std::uint32_t queue);
}

namespace {

using AgcDriver::CheckpointKind;

int failures = 0;

void Expect(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
}

struct Event {
    std::string what;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    std::uintptr_t marker = 0;
};

std::vector<Event> events;

void Log(const char* what, VkCommandBuffer commands) {
    events.push_back({what, commands, 0});
}

VkDevice Device() {
    return reinterpret_cast<VkDevice>(static_cast<std::uintptr_t>(0xD0));
}

VkCommandBuffer Commands(std::uintptr_t value) {
    return reinterpret_cast<VkCommandBuffer>(value);
}

VKAPI_ATTR void VKAPI_CALL mockSetCheckpoint(VkCommandBuffer commands, const void* marker) {
    events.push_back({"checkpoint", commands, reinterpret_cast<std::uintptr_t>(marker)});
}

std::map<std::string, PFN_vkVoidFunction>& Functions() {
    static std::map<std::string, PFN_vkVoidFunction> functions = [] {
        std::map<std::string, PFN_vkVoidFunction> made;
        const auto add = [&](const char* name, auto function) { made[name] = reinterpret_cast<PFN_vkVoidFunction>(function); };
        add("vkCmdSetCheckpointNV", &mockSetCheckpoint);
        add("vkBeginCommandBuffer", static_cast<PFN_vkBeginCommandBuffer>([](VkCommandBuffer c, const VkCommandBufferBeginInfo*) { Log("vkBeginCommandBuffer", c); return VK_SUCCESS; }));
        add("vkEndCommandBuffer", static_cast<PFN_vkEndCommandBuffer>([](VkCommandBuffer c) { Log("vkEndCommandBuffer", c); return VK_SUCCESS; }));
        add("vkCmdDraw", static_cast<PFN_vkCmdDraw>([](VkCommandBuffer c, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) { Log("vkCmdDraw", c); }));
        add("vkCmdDrawIndexed", static_cast<PFN_vkCmdDrawIndexed>([](VkCommandBuffer c, std::uint32_t, std::uint32_t, std::uint32_t, std::int32_t, std::uint32_t) { Log("vkCmdDrawIndexed", c); }));
        add("vkCmdDrawIndirect", static_cast<PFN_vkCmdDrawIndirect>([](VkCommandBuffer c, VkBuffer, VkDeviceSize, std::uint32_t, std::uint32_t) { Log("vkCmdDrawIndirect", c); }));
        add("vkCmdDrawIndexedIndirect", static_cast<PFN_vkCmdDrawIndexedIndirect>([](VkCommandBuffer c, VkBuffer, VkDeviceSize, std::uint32_t, std::uint32_t) { Log("vkCmdDrawIndexedIndirect", c); }));
        add("vkCmdDrawIndirectCountKHR", static_cast<PFN_vkCmdDrawIndirectCountKHR>([](VkCommandBuffer c, VkBuffer, VkDeviceSize, VkBuffer, VkDeviceSize, std::uint32_t, std::uint32_t) { Log("vkCmdDrawIndirectCountKHR", c); }));
        add("vkCmdDrawIndexedIndirectCountKHR", static_cast<PFN_vkCmdDrawIndexedIndirectCountKHR>([](VkCommandBuffer c, VkBuffer, VkDeviceSize, VkBuffer, VkDeviceSize, std::uint32_t, std::uint32_t) { Log("vkCmdDrawIndexedIndirectCountKHR", c); }));
        add("vkCmdDrawMeshTasksEXT", static_cast<PFN_vkCmdDrawMeshTasksEXT>([](VkCommandBuffer c, std::uint32_t, std::uint32_t, std::uint32_t) { Log("vkCmdDrawMeshTasksEXT", c); }));
        add("vkCmdDrawMeshTasksIndirectEXT", static_cast<PFN_vkCmdDrawMeshTasksIndirectEXT>([](VkCommandBuffer c, VkBuffer, VkDeviceSize, std::uint32_t, std::uint32_t) { Log("vkCmdDrawMeshTasksIndirectEXT", c); }));
        add("vkCmdDispatch", static_cast<PFN_vkCmdDispatch>([](VkCommandBuffer c, std::uint32_t, std::uint32_t, std::uint32_t) { Log("vkCmdDispatch", c); }));
        add("vkCmdDispatchBase", static_cast<PFN_vkCmdDispatchBase>([](VkCommandBuffer c, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) { Log("vkCmdDispatchBase", c); }));
        add("vkCmdDispatchIndirect", static_cast<PFN_vkCmdDispatchIndirect>([](VkCommandBuffer c, VkBuffer, VkDeviceSize) { Log("vkCmdDispatchIndirect", c); }));
        add("vkCmdCopyBuffer", static_cast<PFN_vkCmdCopyBuffer>([](VkCommandBuffer c, VkBuffer, VkBuffer, std::uint32_t, const VkBufferCopy*) { Log("vkCmdCopyBuffer", c); }));
        add("vkCmdCopyImage", static_cast<PFN_vkCmdCopyImage>([](VkCommandBuffer c, VkImage, VkImageLayout, VkImage, VkImageLayout, std::uint32_t, const VkImageCopy*) { Log("vkCmdCopyImage", c); }));
        add("vkCmdCopyBufferToImage", static_cast<PFN_vkCmdCopyBufferToImage>([](VkCommandBuffer c, VkBuffer, VkImage, VkImageLayout, std::uint32_t, const VkBufferImageCopy*) { Log("vkCmdCopyBufferToImage", c); }));
        add("vkCmdCopyImageToBuffer", static_cast<PFN_vkCmdCopyImageToBuffer>([](VkCommandBuffer c, VkImage, VkImageLayout, VkBuffer, std::uint32_t, const VkBufferImageCopy*) { Log("vkCmdCopyImageToBuffer", c); }));
        add("vkCmdBlitImage", static_cast<PFN_vkCmdBlitImage>([](VkCommandBuffer c, VkImage, VkImageLayout, VkImage, VkImageLayout, std::uint32_t, const VkImageBlit*, VkFilter) { Log("vkCmdBlitImage", c); }));
        add("vkCmdResolveImage", static_cast<PFN_vkCmdResolveImage>([](VkCommandBuffer c, VkImage, VkImageLayout, VkImage, VkImageLayout, std::uint32_t, const VkImageResolve*) { Log("vkCmdResolveImage", c); }));
        add("vkCmdClearColorImage", static_cast<PFN_vkCmdClearColorImage>([](VkCommandBuffer c, VkImage, VkImageLayout, const VkClearColorValue*, std::uint32_t, const VkImageSubresourceRange*) { Log("vkCmdClearColorImage", c); }));
        add("vkCmdClearDepthStencilImage", static_cast<PFN_vkCmdClearDepthStencilImage>([](VkCommandBuffer c, VkImage, VkImageLayout, const VkClearDepthStencilValue*, std::uint32_t, const VkImageSubresourceRange*) { Log("vkCmdClearDepthStencilImage", c); }));
        add("vkCmdClearAttachments", static_cast<PFN_vkCmdClearAttachments>([](VkCommandBuffer c, std::uint32_t, const VkClearAttachment*, std::uint32_t, const VkClearRect*) { Log("vkCmdClearAttachments", c); }));
        add("vkCmdFillBuffer", static_cast<PFN_vkCmdFillBuffer>([](VkCommandBuffer c, VkBuffer, VkDeviceSize, VkDeviceSize, std::uint32_t) { Log("vkCmdFillBuffer", c); }));
        add("vkCmdUpdateBuffer", static_cast<PFN_vkCmdUpdateBuffer>([](VkCommandBuffer c, VkBuffer, VkDeviceSize, VkDeviceSize, const void*) { Log("vkCmdUpdateBuffer", c); }));
        add("vkCmdPipelineBarrier", static_cast<PFN_vkCmdPipelineBarrier>([](VkCommandBuffer c, VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags, std::uint32_t, const VkMemoryBarrier*, std::uint32_t, const VkBufferMemoryBarrier*, std::uint32_t, const VkImageMemoryBarrier*) { Log("vkCmdPipelineBarrier", c); }));
        add("vkCmdPipelineBarrier2", static_cast<PFN_vkCmdPipelineBarrier2>([](VkCommandBuffer c, const VkDependencyInfo*) { Log("vkCmdPipelineBarrier2", c); }));
        add("vkCmdBeginRenderPass", static_cast<PFN_vkCmdBeginRenderPass>([](VkCommandBuffer c, const VkRenderPassBeginInfo*, VkSubpassContents) { Log("vkCmdBeginRenderPass", c); }));
        add("vkCmdEndRenderPass", static_cast<PFN_vkCmdEndRenderPass>([](VkCommandBuffer c) { Log("vkCmdEndRenderPass", c); }));
        add("vkCmdBeginRenderingKHR", static_cast<PFN_vkCmdBeginRendering>([](VkCommandBuffer c, const VkRenderingInfo*) { Log("vkCmdBeginRendering", c); }));
        add("vkCmdEndRenderingKHR", static_cast<PFN_vkCmdEndRendering>([](VkCommandBuffer c) { Log("vkCmdEndRendering", c); }));
        add("vkCmdResetQueryPool", static_cast<PFN_vkCmdResetQueryPool>([](VkCommandBuffer c, VkQueryPool, std::uint32_t, std::uint32_t) { Log("vkCmdResetQueryPool", c); }));
        add("vkCmdBeginQuery", static_cast<PFN_vkCmdBeginQuery>([](VkCommandBuffer c, VkQueryPool, std::uint32_t, VkQueryControlFlags) { Log("vkCmdBeginQuery", c); }));
        add("vkCmdEndQuery", static_cast<PFN_vkCmdEndQuery>([](VkCommandBuffer c, VkQueryPool, std::uint32_t) { Log("vkCmdEndQuery", c); }));
        add("vkCmdCopyQueryPoolResults", static_cast<PFN_vkCmdCopyQueryPoolResults>([](VkCommandBuffer c, VkQueryPool, std::uint32_t, std::uint32_t, VkBuffer, VkDeviceSize, VkDeviceSize, VkQueryResultFlags) { Log("vkCmdCopyQueryPoolResults", c); }));
        add("vkCmdWriteTimestamp", static_cast<PFN_vkCmdWriteTimestamp>([](VkCommandBuffer c, VkPipelineStageFlagBits, VkQueryPool, std::uint32_t) { Log("vkCmdWriteTimestamp", c); }));
        add("vkCmdExecuteCommands", static_cast<PFN_vkCmdExecuteCommands>([](VkCommandBuffer c, std::uint32_t, const VkCommandBuffer*) { Log("vkCmdExecuteCommands", c); }));
        add("vkCmdBindPipeline", static_cast<PFN_vkCmdBindPipeline>([](VkCommandBuffer c, VkPipelineBindPoint, VkPipeline) { Log("vkCmdBindPipeline", c); }));
        add("vkCmdSetViewport", static_cast<PFN_vkCmdSetViewport>([](VkCommandBuffer c, std::uint32_t, std::uint32_t, const VkViewport*) { Log("vkCmdSetViewport", c); }));
        add("vkCmdPushConstants", static_cast<PFN_vkCmdPushConstants>([](VkCommandBuffer c, VkPipelineLayout, VkShaderStageFlags, std::uint32_t, std::uint32_t, const void*) { Log("vkCmdPushConstants", c); }));
        add("vkCmdWriteBufferMarkerAMD", static_cast<PFN_vkCmdWriteBufferMarkerAMD>([](VkCommandBuffer c, VkPipelineStageFlagBits, VkBuffer, VkDeviceSize, std::uint32_t) { Log("vkCmdWriteBufferMarkerAMD", c); }));
        add("vkCmdNextSubpass", static_cast<PFN_vkCmdNextSubpass>([](VkCommandBuffer c, VkSubpassContents) { Log("vkCmdNextSubpass", c); }));
        add("vkCmdSetEvent", static_cast<PFN_vkCmdSetEvent>([](VkCommandBuffer c, VkEvent, VkPipelineStageFlags) { Log("vkCmdSetEvent", c); }));
        add("vkCmdResetEvent", static_cast<PFN_vkCmdResetEvent>([](VkCommandBuffer c, VkEvent, VkPipelineStageFlags) { Log("vkCmdResetEvent", c); }));
        add("vkCmdWaitEvents", static_cast<PFN_vkCmdWaitEvents>([](VkCommandBuffer c, std::uint32_t, const VkEvent*, VkPipelineStageFlags, VkPipelineStageFlags, std::uint32_t, const VkMemoryBarrier*, std::uint32_t, const VkBufferMemoryBarrier*, std::uint32_t, const VkImageMemoryBarrier*) { Log("vkCmdWaitEvents", c); }));
        add("vkCmdSetEvent2", static_cast<PFN_vkCmdSetEvent2>([](VkCommandBuffer c, VkEvent, const VkDependencyInfo*) { Log("vkCmdSetEvent2", c); }));
        add("vkCmdResetEvent2", static_cast<PFN_vkCmdResetEvent2>([](VkCommandBuffer c, VkEvent, VkPipelineStageFlags2) { Log("vkCmdResetEvent2", c); }));
        add("vkCmdWaitEvents2", static_cast<PFN_vkCmdWaitEvents2>([](VkCommandBuffer c, std::uint32_t, const VkEvent*, const VkDependencyInfo*) { Log("vkCmdWaitEvents2", c); }));
        add("vkDestroyBuffer", static_cast<PFN_vkDestroyBuffer>([](VkDevice, VkBuffer, const VkAllocationCallbacks*) {}));
        return made;
    }();
    return functions;
}

std::map<std::string, PFN_vkVoidFunction> splits;
bool anyCommand = false;
bool otherCheckpoint = false;
const char* missing = nullptr;

template<typename TFunction>
struct Other;

template<typename... TArguments>
struct Other<void (VKAPI_PTR*)(VkCommandBuffer, TArguments...)> {
    static VKAPI_ATTR void VKAPI_CALL Call(VkCommandBuffer commands, TArguments...) {
        Log("other", commands);
    }
};

template<typename TFunction>
PFN_vkVoidFunction OtherFunction() {
    return reinterpret_cast<PFN_vkVoidFunction>(&Other<TFunction>::Call);
}

VKAPI_ATTR void VKAPI_CALL otherSetCheckpoint(VkCommandBuffer, const void*) {
}

const char* Canonical(const char* name) {
    static const std::map<std::string, const char*> aliases = {
        {"vkCmdDrawIndirectCount", "vkCmdDrawIndirectCountKHR"},
        {"vkCmdDrawIndexedIndirectCount", "vkCmdDrawIndexedIndirectCountKHR"},
        {"vkCmdBeginRendering", "vkCmdBeginRenderingKHR"},
        {"vkCmdEndRendering", "vkCmdEndRenderingKHR"},
        {"vkCmdPipelineBarrier2KHR", "vkCmdPipelineBarrier2"},
        {"vkCmdSetEvent2KHR", "vkCmdSetEvent2"},
        {"vkCmdResetEvent2KHR", "vkCmdResetEvent2"},
        {"vkCmdWaitEvents2KHR", "vkCmdWaitEvents2"},
    };
    const auto found = aliases.find(name);
    return found == aliases.end() ? name : found->second;
}

VKAPI_ATTR void VKAPI_CALL anySourceCommand() {
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL mockProc(VkDevice, const char* name) {
    if (missing != nullptr && std::strcmp(name, missing) == 0) return nullptr;
    if (otherCheckpoint && std::strcmp(name, "vkCmdSetCheckpointNV") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&otherSetCheckpoint);
    if (const auto split = splits.find(name); split != splits.end()) return split->second;
    const auto found = Functions().find(Canonical(name));
    if (found != Functions().end()) return found->second;
    return anyCommand && std::strncmp(name, "vkCmd", 5) == 0 ? reinterpret_cast<PFN_vkVoidFunction>(&anySourceCommand) : nullptr;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL otherProc(VkDevice device, const char* name) {
    return mockProc(device, name);
}

template<typename TFunction>
TFunction Resolve(PFN_vkGetDeviceProcAddr proc, const char* name) {
    return reinterpret_cast<TFunction>(proc(Device(), name));
}

AgcDriver::FaultToken TokenOf(const Event& event) {
    return static_cast<AgcDriver::FaultToken>(event.marker);
}

CheckpointKind KindOf(const Event& event) {
    return AgcDriver::CheckpointKindOf(reinterpret_cast<const void*>(event.marker));
}

void CheckCommand(PFN_vkGetDeviceProcAddr proc, const char* name, CheckpointKind kind, void (*call)(PFN_vkVoidFunction function, VkCommandBuffer commands), const char* logged = nullptr) {
    const auto wrapper = proc(Device(), name);
    Expect(wrapper != nullptr && wrapper != Functions()[Canonical(name)], std::string(name) + " was not wrapped");
    if (wrapper == nullptr) return;
    events.clear();
    call(wrapper, Commands(0x10));
    const std::string real = logged != nullptr ? logged : name;
    Expect(events.size() == 2 && events[0].what == "checkpoint" && events[1].what == real, std::string(name) + " did not record a checkpoint and then itself");
    if (events.size() == 2 && events[0].what == "checkpoint") {
        Expect(KindOf(events[0]) == kind, std::string(name) + " recorded the kind " + AgcDriver::CheckpointKindName(KindOf(events[0])));
        Expect(events[0].commands == Commands(0x10), std::string(name) + " recorded its checkpoint into another command buffer");
    }
    events.clear();
    AgcDriver::Graphics::NoteRecordedCheckpoint(Commands(0x10), 9);
    call(wrapper, Commands(0x10));
    if (kind == CheckpointKind::draw || kind == CheckpointKind::dispatch) {
        Expect(events.size() == 1 && events[0].what == real, std::string(name) + " got a second checkpoint right after a guest token");
    } else {
        Expect(events.size() == 2 && KindOf(events[0]) == kind && TokenOf(events[0]) == 9, std::string(name) + " lost its checkpoint after a guest token");
    }
}

template<typename TFunction>
TFunction As(PFN_vkVoidFunction function) {
    return reinterpret_cast<TFunction>(function);
}

void EveryCommandRecordsItsKind(PFN_vkGetDeviceProcAddr proc) {
    CheckCommand(proc, "vkCmdDraw", CheckpointKind::draw, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdDraw>(f)(c, 3, 1, 0, 0); });
    CheckCommand(proc, "vkCmdDrawIndexed", CheckpointKind::draw, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdDrawIndexed>(f)(c, 3, 1, 0, 0, 0); });
    CheckCommand(proc, "vkCmdDrawIndirect", CheckpointKind::draw, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdDrawIndirect>(f)(c, VK_NULL_HANDLE, 0, 1, 16); });
    CheckCommand(proc, "vkCmdDrawIndexedIndirect", CheckpointKind::draw, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdDrawIndexedIndirect>(f)(c, VK_NULL_HANDLE, 0, 1, 20); });
    CheckCommand(proc, "vkCmdDrawIndirectCountKHR", CheckpointKind::draw, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdDrawIndirectCountKHR>(f)(c, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 0, 1, 16); });
    CheckCommand(proc, "vkCmdDrawIndexedIndirectCountKHR", CheckpointKind::draw, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdDrawIndexedIndirectCountKHR>(f)(c, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 0, 1, 20); });
    CheckCommand(proc, "vkCmdDrawMeshTasksEXT", CheckpointKind::draw, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdDrawMeshTasksEXT>(f)(c, 1, 1, 1); });
    CheckCommand(proc, "vkCmdDrawMeshTasksIndirectEXT", CheckpointKind::draw, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdDrawMeshTasksIndirectEXT>(f)(c, VK_NULL_HANDLE, 0, 1, 12); });
    CheckCommand(proc, "vkCmdDispatch", CheckpointKind::dispatch, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdDispatch>(f)(c, 1, 1, 1); });
    CheckCommand(proc, "vkCmdDispatchBase", CheckpointKind::dispatch, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdDispatchBase>(f)(c, 4, 0, 0, 1, 1, 1); });
    CheckCommand(proc, "vkCmdDispatchIndirect", CheckpointKind::dispatch, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdDispatchIndirect>(f)(c, VK_NULL_HANDLE, 0); });
    CheckCommand(proc, "vkCmdCopyBuffer", CheckpointKind::copy, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdCopyBuffer>(f)(c, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, nullptr); });
    CheckCommand(proc, "vkCmdCopyImage", CheckpointKind::copy, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdCopyImage>(f)(c, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, 0, nullptr); });
    CheckCommand(proc, "vkCmdCopyBufferToImage", CheckpointKind::copy, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdCopyBufferToImage>(f)(c, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, 0, nullptr); });
    CheckCommand(proc, "vkCmdCopyImageToBuffer", CheckpointKind::copy, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdCopyImageToBuffer>(f)(c, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE, 0, nullptr); });
    CheckCommand(proc, "vkCmdBlitImage", CheckpointKind::blit, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdBlitImage>(f)(c, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, 0, nullptr, VK_FILTER_NEAREST); });
    CheckCommand(proc, "vkCmdResolveImage", CheckpointKind::blit, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdResolveImage>(f)(c, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, 0, nullptr); });
    CheckCommand(proc, "vkCmdClearColorImage", CheckpointKind::clear, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdClearColorImage>(f)(c, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, nullptr, 0, nullptr); });
    CheckCommand(proc, "vkCmdClearDepthStencilImage", CheckpointKind::clear, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdClearDepthStencilImage>(f)(c, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, nullptr, 0, nullptr); });
    CheckCommand(proc, "vkCmdClearAttachments", CheckpointKind::clear, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdClearAttachments>(f)(c, 0, nullptr, 0, nullptr); });
    CheckCommand(proc, "vkCmdFillBuffer", CheckpointKind::fill, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdFillBuffer>(f)(c, VK_NULL_HANDLE, 0, 4, 0); });
    CheckCommand(proc, "vkCmdUpdateBuffer", CheckpointKind::update, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdUpdateBuffer>(f)(c, VK_NULL_HANDLE, 0, 4, nullptr); });
    CheckCommand(proc, "vkCmdPipelineBarrier", CheckpointKind::barrier, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdPipelineBarrier>(f)(c, 0, 0, 0, 0, nullptr, 0, nullptr, 0, nullptr); });
    CheckCommand(proc, "vkCmdPipelineBarrier2", CheckpointKind::barrier, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdPipelineBarrier2>(f)(c, nullptr); });
    CheckCommand(proc, "vkCmdBeginRenderPass", CheckpointKind::renderPassBegin, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdBeginRenderPass>(f)(c, nullptr, VK_SUBPASS_CONTENTS_INLINE); });
    CheckCommand(proc, "vkCmdEndRenderPass", CheckpointKind::renderPassEnd, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdEndRenderPass>(f)(c); });
    CheckCommand(proc, "vkCmdBeginRendering", CheckpointKind::renderPassBegin, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdBeginRendering>(f)(c, nullptr); });
    CheckCommand(proc, "vkCmdEndRendering", CheckpointKind::renderPassEnd, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdEndRendering>(f)(c); });
    CheckCommand(proc, "vkCmdResetQueryPool", CheckpointKind::query, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdResetQueryPool>(f)(c, VK_NULL_HANDLE, 0, 1); });
    CheckCommand(proc, "vkCmdBeginQuery", CheckpointKind::query, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdBeginQuery>(f)(c, VK_NULL_HANDLE, 0, 0); });
    CheckCommand(proc, "vkCmdEndQuery", CheckpointKind::query, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdEndQuery>(f)(c, VK_NULL_HANDLE, 0); });
    CheckCommand(proc, "vkCmdCopyQueryPoolResults", CheckpointKind::query, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdCopyQueryPoolResults>(f)(c, VK_NULL_HANDLE, 0, 1, VK_NULL_HANDLE, 0, 8, 0); });
    CheckCommand(proc, "vkCmdWriteTimestamp", CheckpointKind::query, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdWriteTimestamp>(f)(c, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_NULL_HANDLE, 0); });
    CheckCommand(proc, "vkCmdExecuteCommands", CheckpointKind::execute, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdExecuteCommands>(f)(c, 0, nullptr); });
    CheckCommand(proc, "vkCmdSetEvent", CheckpointKind::event, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdSetEvent>(f)(c, VK_NULL_HANDLE, VK_PIPELINE_STAGE_TRANSFER_BIT); });
    CheckCommand(proc, "vkCmdResetEvent", CheckpointKind::event, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdResetEvent>(f)(c, VK_NULL_HANDLE, VK_PIPELINE_STAGE_TRANSFER_BIT); });
    CheckCommand(proc, "vkCmdWaitEvents", CheckpointKind::wait, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdWaitEvents>(f)(c, 0, nullptr, 0, 0, 0, nullptr, 0, nullptr, 0, nullptr); });
    CheckCommand(proc, "vkCmdSetEvent2", CheckpointKind::event, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdSetEvent2>(f)(c, VK_NULL_HANDLE, nullptr); });
    CheckCommand(proc, "vkCmdResetEvent2", CheckpointKind::event, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdResetEvent2>(f)(c, VK_NULL_HANDLE, 0); });
    CheckCommand(proc, "vkCmdWaitEvents2", CheckpointKind::wait, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdWaitEvents2>(f)(c, 0, nullptr, nullptr); });
    CheckCommand(proc, "vkEndCommandBuffer", CheckpointKind::commandBufferEnd, [](PFN_vkVoidFunction f, VkCommandBuffer c) { static_cast<void>(As<PFN_vkEndCommandBuffer>(f)(c)); });
    const auto begin = Resolve<PFN_vkBeginCommandBuffer>(proc, "vkBeginCommandBuffer");
    events.clear();
    const VkCommandBufferBeginInfo info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    Expect(begin(Commands(0x10), &info) == VK_SUCCESS, "vkBeginCommandBuffer lost its result");
    Expect(events.size() == 2 && events[0].what == "vkBeginCommandBuffer" && events[1].what == "checkpoint" && KindOf(events[1]) == CheckpointKind::commandBufferBegin, "vkBeginCommandBuffer did not record a checkpoint after beginning");
}

void CheckpointsCarryTheGuestToken(PFN_vkGetDeviceProcAddr proc) {
    const auto barrier = Resolve<PFN_vkCmdPipelineBarrier>(proc, "vkCmdPipelineBarrier");
    const auto dispatch = Resolve<PFN_vkCmdDispatch>(proc, "vkCmdDispatch");
    const auto begin = Resolve<PFN_vkBeginCommandBuffer>(proc, "vkBeginCommandBuffer");
    const auto draw = Resolve<PFN_vkCmdDraw>(proc, "vkCmdDraw");
    events.clear();
    AgcDriver::Graphics::NoteRecordedCheckpoint(Commands(0x20), 77);
    dispatch(Commands(0x30), 8, 1, 1);
    dispatch(Commands(0x20), 32, 1, 1);
    dispatch(Commands(0x20), 16, 1, 1);
    barrier(Commands(0x20), 0, 0, 0, 0, nullptr, 0, nullptr, 0, nullptr);
    barrier(Commands(0x30), 0, 0, 0, 0, nullptr, 0, nullptr, 0, nullptr);
    Expect(events.size() == 9, "the token checks did not record four checkpoints and five commands");
    if (events.size() == 9) {
        Expect(events[0].what == "checkpoint" && KindOf(events[0]) == CheckpointKind::dispatch && TokenOf(events[0]) == 0, "a dispatch on another command buffer lost its checkpoint after a guest token");
        Expect(events[2].what == "vkCmdDispatch", "a guest dispatch got a second checkpoint right after its guest token");
        Expect(KindOf(events[3]) == CheckpointKind::dispatch && TokenOf(events[3]) == 77, "a second dispatch after a guest token does not carry it");
        Expect(KindOf(events[5]) == CheckpointKind::barrier && TokenOf(events[5]) == 77, "a barrier after a guest token does not carry it");
        Expect(TokenOf(events[7]) == 0, "a checkpoint on another command buffer carries its guest token");
    }
    events.clear();
    AgcDriver::Graphics::NoteRecordedCheckpoint(Commands(0x20), 78);
    barrier(Commands(0x20), 0, 0, 0, 0, nullptr, 0, nullptr, 0, nullptr);
    draw(Commands(0x20), 3, 1, 0, 0);
    Expect(events.size() == 4 && KindOf(events[2]) == CheckpointKind::draw && TokenOf(events[2]) == 78, "a draw after a guest token and a barrier lost its checkpoint");
    const VkCommandBufferBeginInfo info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    static_cast<void>(begin(Commands(0x20), &info));
    events.clear();
    barrier(Commands(0x20), 0, 0, 0, 0, nullptr, 0, nullptr, 0, nullptr);
    Expect(!events.empty() && TokenOf(events[0]) == 0, "a guest token outlived vkBeginCommandBuffer of its command buffer");
    AgcDriver::Graphics::NoteRecordedCheckpoint(Commands(0x20), 80);
    static_cast<void>(begin(Commands(0x20), &info));
    events.clear();
    dispatch(Commands(0x20), 4, 1, 1);
    Expect(events.size() == 2 && KindOf(events[0]) == CheckpointKind::dispatch, "a dispatch after vkBeginCommandBuffer lost its checkpoint to an earlier guest token");
}

void RecorderNotesItsToken(PFN_vkGetDeviceProcAddr proc) {
    AgcDriver::Graphics::Context context{};
    context.device = Device();
    context.queue = reinterpret_cast<VkQueue>(static_cast<std::uintptr_t>(0x900));
    context.deviceProc = proc;
    events.clear();
    AgcDriver::Graphics::WriteNvCheckpointToken(context, Commands(0x50), 4242, 0);
    Expect(events.size() == 1 && events[0].what == "checkpoint" && TokenOf(events[0]) == 4242 && KindOf(events[0]) == CheckpointKind::none, "the recorder did not write its guest token");
    Resolve<PFN_vkCmdFillBuffer>(proc, "vkCmdFillBuffer")(Commands(0x50), VK_NULL_HANDLE, 0, 4, 0);
    Expect(events.size() == 3 && TokenOf(events[1]) == 4242 && KindOf(events[1]) == CheckpointKind::fill, "a command after the recorder's guest token does not carry it");
    events.clear();
    AgcDriver::Graphics::WriteNvCheckpointToken(context, Commands(0x50), 4243, 0);
    Resolve<PFN_vkCmdDraw>(proc, "vkCmdDraw")(Commands(0x50), 3, 1, 0, 0);
    Expect(events.size() == 2 && TokenOf(events[0]) == 4243 && events[1].what == "vkCmdDraw", "a guest draw got a second checkpoint right after the recorder's guest token");
}

void PassesStateThroughAndRejectsUnknownCommands(PFN_vkGetDeviceProcAddr proc) {
    for (const char* name : {"vkCmdBindPipeline", "vkCmdSetViewport", "vkCmdPushConstants", "vkCmdWriteBufferMarkerAMD", "vkCmdSetCheckpointNV", "vkDestroyBuffer"}) {
        Expect(proc(Device(), name) == Functions()[name], std::string(name) + " did not pass through");
    }
    Expect(proc(Device(), "vkCmdTraceRaysKHR") == nullptr, "a command the device lacks did not resolve to null");
    missing = "vkCmdDraw";
    Expect(proc(Device(), "vkCmdDraw") == nullptr, "a wrapped command the device lacks did not resolve to null");
    missing = nullptr;
    bool threw = false;
    try {
        static_cast<void>(proc(Device(), "vkCmdNextSubpass"));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Expect(threw, "an unknown work command passed through without a checkpoint");
}

bool ThrowsMentioning(PFN_vkGetDeviceProcAddr resolver, const char* text) {
    try {
        static_cast<void>(AgcDriver::Graphics::CheckpointCommandKinds(Device(), resolver));
    } catch (const std::runtime_error& error) {
        return std::string(error.what()).find(text) != std::string::npos;
    }
    return false;
}

void RejectsMismatchedSetups() {
    missing = "vkCmdSetCheckpointNV";
    Expect(ThrowsMentioning(&mockProc, "missing Vulkan function: vkCmdSetCheckpointNV"), "command checkpoints were set up without vkCmdSetCheckpointNV");
    missing = nullptr;
    otherCheckpoint = true;
    Expect(ThrowsMentioning(&mockProc, "two different vkCmdSetCheckpointNV"), "a second, different vkCmdSetCheckpointNV was accepted");
    otherCheckpoint = false;
    bool threw = false;
    try {
        static_cast<void>(AgcDriver::Graphics::CheckpointCommandKinds(Device(), &otherProc));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Expect(threw, "a second, different device resolver was accepted");
    Expect(AgcDriver::Graphics::CheckpointCommandKinds(Device(), &mockProc) != nullptr, "the same resolver was refused when set up again");
}

void RejectsTwoFunctionsForOneCommand(PFN_vkGetDeviceProcAddr proc) {
    static_cast<void>(proc(Device(), "vkCmdDispatch"));
    Expect(proc(Device(), "vkCmdDispatch") != nullptr, "resolving one command twice was refused");
    splits["vkCmdDispatch"] = OtherFunction<PFN_vkCmdDispatch>();
    bool threw = false;
    try {
        static_cast<void>(proc(Device(), "vkCmdDispatch"));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    splits.erase("vkCmdDispatch");
    Expect(threw, "a command resolving to a second, different function was accepted");
}

void KeepsAliasesApart(PFN_vkGetDeviceProcAddr proc) {
    splits["vkCmdDrawIndirectCount"] = OtherFunction<PFN_vkCmdDrawIndirectCountKHR>();
    splits["vkCmdDrawIndexedIndirectCount"] = OtherFunction<PFN_vkCmdDrawIndexedIndirectCountKHR>();
    splits["vkCmdPipelineBarrier2KHR"] = OtherFunction<PFN_vkCmdPipelineBarrier2>();
    splits["vkCmdBeginRenderingKHR"] = OtherFunction<PFN_vkCmdBeginRendering>();
    splits["vkCmdEndRenderingKHR"] = OtherFunction<PFN_vkCmdEndRendering>();
    splits["vkCmdSetEvent2KHR"] = OtherFunction<PFN_vkCmdSetEvent2>();
    splits["vkCmdResetEvent2KHR"] = OtherFunction<PFN_vkCmdResetEvent2>();
    splits["vkCmdWaitEvents2KHR"] = OtherFunction<PFN_vkCmdWaitEvents2>();
    CheckCommand(proc, "vkCmdDrawIndirectCount", CheckpointKind::draw, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdDrawIndirectCountKHR>(f)(c, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 0, 1, 16); }, "other");
    CheckCommand(proc, "vkCmdDrawIndexedIndirectCount", CheckpointKind::draw, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdDrawIndexedIndirectCountKHR>(f)(c, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 0, 1, 20); }, "other");
    CheckCommand(proc, "vkCmdPipelineBarrier2KHR", CheckpointKind::barrier, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdPipelineBarrier2>(f)(c, nullptr); }, "other");
    CheckCommand(proc, "vkCmdBeginRenderingKHR", CheckpointKind::renderPassBegin, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdBeginRendering>(f)(c, nullptr); }, "other");
    CheckCommand(proc, "vkCmdEndRenderingKHR", CheckpointKind::renderPassEnd, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdEndRendering>(f)(c); }, "other");
    CheckCommand(proc, "vkCmdSetEvent2KHR", CheckpointKind::event, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdSetEvent2>(f)(c, VK_NULL_HANDLE, nullptr); }, "other");
    CheckCommand(proc, "vkCmdResetEvent2KHR", CheckpointKind::event, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdResetEvent2>(f)(c, VK_NULL_HANDLE, 0); }, "other");
    CheckCommand(proc, "vkCmdWaitEvents2KHR", CheckpointKind::wait, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdWaitEvents2>(f)(c, 0, nullptr, nullptr); }, "other");
    CheckCommand(proc, "vkCmdPipelineBarrier2", CheckpointKind::barrier, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdPipelineBarrier2>(f)(c, nullptr); });
    CheckCommand(proc, "vkCmdDrawIndirectCountKHR", CheckpointKind::draw, [](PFN_vkVoidFunction f, VkCommandBuffer c) { As<PFN_vkCmdDrawIndirectCountKHR>(f)(c, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 0, 1, 16); });
}

void ClassifiesEverySourceCommand(PFN_vkGetDeviceProcAddr proc, const std::filesystem::path& root) {
    const std::regex literal("\"(vkCmd[A-Za-z0-9]+)\"");
    const std::regex type("PFN_(vkCmd[A-Za-z0-9]+)");
    const std::regex macro("X\\((Cmd[A-Za-z0-9]+)\\)");
    std::set<std::string> names;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) continue;
        const auto relative = std::filesystem::relative(entry.path(), root).generic_string();
        if (relative.rfind("tests/", 0) == 0) continue;
        if (relative == "Graphics/src/CommandCheckpoints.cpp") continue;
        const auto extension = entry.path().extension().string();
        if (extension != ".cpp" && extension != ".hpp" && extension != ".h" && extension != ".inl") continue;
        std::ifstream file(entry.path(), std::ios::binary);
        std::stringstream text;
        text << file.rdbuf();
        const auto content = text.str();
        for (std::sregex_iterator match(content.begin(), content.end(), literal), end; match != end; ++match) names.insert((*match)[1].str());
        for (std::sregex_iterator match(content.begin(), content.end(), type), end; match != end; ++match) names.insert((*match)[1].str());
        for (std::sregex_iterator match(content.begin(), content.end(), macro), end; match != end; ++match) names.insert("vk" + (*match)[1].str());
    }
    Expect(names.size() > 20, "the source scan found too few vkCmd names under " + root.generic_string());
    anyCommand = true;
    for (const auto& name : names) {
        try {
            static_cast<void>(proc(Device(), name.c_str()));
        } catch (const std::runtime_error& error) {
            Expect(false, name + " in the driver sources is not classified: " + error.what());
        }
    }
    anyCommand = false;
}

}

int main(int argc, char** argv) {
    try {
        const auto proc = AgcDriver::Graphics::CheckpointCommandKinds(Device(), &mockProc);
        Expect(proc != nullptr && proc != &mockProc, "command checkpoints did not return their own resolver");
        EveryCommandRecordsItsKind(proc);
        CheckpointsCarryTheGuestToken(proc);
        RecorderNotesItsToken(proc);
        PassesStateThroughAndRejectsUnknownCommands(proc);
        RejectsMismatchedSetups();
        RejectsTwoFunctionsForOneCommand(proc);
        KeepsAliasesApart(proc);
        Expect(argc == 2, "the test needs the libSceAgcDriver source directory");
        if (argc == 2) ClassifiesEverySourceCommand(proc, argv[1]);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "agc_command_checkpoints: %s\n", error.what());
        return 1;
    }
    if (failures != 0) return 1;
    std::puts("agc_command_checkpoints: passed");
    return 0;
}
