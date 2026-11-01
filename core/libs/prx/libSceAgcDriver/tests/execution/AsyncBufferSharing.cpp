#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

using AgcDriver::Graphics::Context;
using AgcDriver::Graphics::Require;

struct Observation {
    VkDevice device = VK_NULL_HANDLE;
    VkBufferCreateInfo info{};
    const VkBufferCreateInfo* original = nullptr;
    const VkAllocationCallbacks* allocator = nullptr;
    std::vector<std::uint32_t> families;
    std::uint32_t calls = 0;
};

Observation first;
Observation second;
VkResult secondResult = VK_SUCCESS;

VkResult capture(Observation& observed, VkDevice device, const VkBufferCreateInfo* info, const VkAllocationCallbacks* allocator, VkBuffer* buffer, VkResult result) {
    observed.device = device;
    observed.info = *info;
    observed.original = info;
    observed.allocator = allocator;
    observed.families.clear();
    if (info->sharingMode == VK_SHARING_MODE_CONCURRENT) observed.families.assign(info->pQueueFamilyIndices, info->pQueueFamilyIndices + info->queueFamilyIndexCount);
    ++observed.calls;
    *buffer = VK_NULL_HANDLE;
    return result;
}

VKAPI_ATTR VkResult VKAPI_CALL createFirst(VkDevice device, const VkBufferCreateInfo* info, const VkAllocationCallbacks* allocator, VkBuffer* buffer) {
    return capture(first, device, info, allocator, buffer, VK_SUCCESS);
}

VKAPI_ATTR VkResult VKAPI_CALL createSecond(VkDevice device, const VkBufferCreateInfo* info, const VkAllocationCallbacks* allocator, VkBuffer* buffer) {
    return capture(second, device, info, allocator, buffer, secondResult);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL resolve(VkDevice device, const char* name) {
    Require(std::strcmp(name, "vkCreateBuffer") == 0, "unexpected device procedure lookup");
    return device == reinterpret_cast<VkDevice>(std::uintptr_t{1}) ? reinterpret_cast<PFN_vkVoidFunction>(createFirst) : reinterpret_cast<PFN_vkVoidFunction>(createSecond);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL overrideResolve(VkDevice, const char* name) {
    Require(std::strcmp(name, "vkCreateBuffer") == 0, "unexpected overridden device procedure lookup");
    return reinterpret_cast<PFN_vkVoidFunction>(createSecond);
}

Context context(std::uintptr_t device, std::uint32_t main, std::uint32_t async) {
    Context result{};
    result.device = reinterpret_cast<VkDevice>(device);
    result.deviceProc = resolve;
    result.queueFamily = main;
    result.asyncFamily = async;
    result.bufferSharingFamilies = {main, async};
    result.bufferSharingFamilyCount = 2;
    return result;
}

void checkFields(const Observation& observed, const VkBufferCreateInfo& input, const VkAllocationCallbacks* allocator) {
    Require(observed.info.sType == input.sType && observed.info.pNext == input.pNext && observed.info.flags == input.flags && observed.info.size == input.size && observed.info.usage == input.usage, "buffer sharing changed creation fields");
    Require(observed.allocator == allocator, "buffer sharing changed allocation callbacks");
}

void sharing() {
    VkExternalMemoryBufferCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO};
    external.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.pNext = &external;
    info.flags = VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_ALIASED_BIT;
    info.size = 16384;
    info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    const auto original = info;
    VkAllocationCallbacks allocator{};
    VkBuffer buffer = VK_NULL_HANDLE;
    auto a = context(1, 0, 3);
    Require(a.CreateBuffer(&info, &allocator, &buffer) == VK_SUCCESS, "exclusive buffer creation failed");
    Require(first.families == std::vector<std::uint32_t>{0, 3}, "exclusive buffer missed both lane families");
    checkFields(first, info, &allocator);
    Require(info.sharingMode == original.sharingMode && info.queueFamilyIndexCount == original.queueFamilyIndexCount && info.pQueueFamilyIndices == original.pQueueFamilyIndices, "buffer helper mutated caller create info");

    const std::uint32_t imported[]{0, 1, 2};
    info.sharingMode = VK_SHARING_MODE_CONCURRENT;
    info.queueFamilyIndexCount = 3;
    info.pQueueFamilyIndices = imported;
    a.CreateBuffer(&info, &allocator, &buffer);
    Require(first.families == std::vector<std::uint32_t>{0, 1, 2, 3}, "DMA-buf main/settle/sparse families were lost");
    checkFields(first, info, &allocator);
    Require(info.queueFamilyIndexCount == 3 && info.pQueueFamilyIndices == imported, "concurrent input was mutated");

    const std::uint32_t repeated[]{2, 0, 2, 3, 1, 3};
    info.queueFamilyIndexCount = 6;
    info.pQueueFamilyIndices = repeated;
    a.CreateBuffer(&info, nullptr, &buffer);
    Require(first.families == std::vector<std::uint32_t>{2, 0, 3, 1}, "duplicate family union changed existing access");

    const std::uint32_t extended[]{0, 1, 2, 4, 5};
    info.queueFamilyIndexCount = 5;
    info.pQueueFamilyIndices = extended;
    a.CreateBuffer(&info, nullptr, &buffer);
    Require(first.families == std::vector<std::uint32_t>{0, 1, 2, 4, 5, 3}, "buffer sharing has a fixed existing-family capacity");

    a.bufferSharingFamilyCount = 0;
    a.CreateBuffer(&info, &allocator, &buffer);
    Require(first.original == &info && first.families == std::vector<std::uint32_t>{0, 1, 2, 4, 5}, "disabled async changed original sharing");
    checkFields(first, info, &allocator);
}

void deviceIsolation() {
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = 256;
    info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer buffer = VK_NULL_HANDLE;
    auto a = context(1, 0, 2);
    auto saved = a;
    a.CreateBuffer(&info, nullptr, &buffer);
    const auto firstCalls = first.calls;
    {
        const auto b = context(2, 6, 9);
        b.CreateBuffer(&info, nullptr, &buffer);
        Require(second.device == b.device && second.families == std::vector<std::uint32_t>{6, 9}, "second device inherited first device sharing");
        Require(first.calls == firstCalls, "second device called the first device function");
        saved.CreateBuffer(&info, nullptr, &buffer);
        Require(first.device == a.device && first.families == std::vector<std::uint32_t>{0, 2}, "second device overwrote first device sharing");
    }
    const auto recreated = context(2, 11, 14);
    recreated.CreateBuffer(&info, nullptr, &buffer);
    Require(second.families == std::vector<std::uint32_t>{11, 14}, "replacement context reused destroyed context families");
    saved.CreateBuffer(&info, nullptr, &buffer);
    Require(first.families == std::vector<std::uint32_t>{0, 2}, "context recreation altered surviving device");

    a.CreateBuffer(&info, nullptr, &buffer);
    Require(first.device == a.device && first.families == std::vector<std::uint32_t>{0, 2}, "lookup used the wrong device");
    auto b = recreated;
    secondResult = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    Require(b.CreateBuffer(&info, nullptr, &buffer) == VK_ERROR_OUT_OF_DEVICE_MEMORY, "buffer helper discarded the device result");
    Require(second.device == b.device && second.families == std::vector<std::uint32_t>{11, 14}, "second device used stale families");
    secondResult = VK_SUCCESS;
}

void contextOverride() {
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = 256;
    info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    VkBuffer buffer = VK_NULL_HANDLE;
    const auto original = context(1, 0, 3);
    original.CreateBuffer(&info, nullptr, &buffer);
    auto overridden = original;
    overridden.deviceProc = overrideResolve;
    for (const auto sharingCount : {2u, 0u}) {
        overridden.bufferSharingFamilyCount = sharingCount;
        const auto firstCalls = first.calls;
        const auto secondCalls = second.calls;
        secondResult = VK_ERROR_OUT_OF_DEVICE_MEMORY;
        Require(overridden.CreateBuffer(&info, nullptr, &buffer) == VK_ERROR_OUT_OF_DEVICE_MEMORY, "per-context create failure injection was bypassed");
        Require(second.calls == secondCalls + 1 && first.calls == firstCalls && second.device == original.device, "copied context ignored its current resolver");
        Require(second.info.sharingMode == (sharingCount == 0 ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT), "resolver override changed sharing policy");
        secondResult = VK_SUCCESS;
        original.CreateBuffer(&info, nullptr, &buffer);
        Require(first.calls == firstCalls + 1 && first.device == original.device, "overridden context changed original device procedure");
    }
}

void auxiliaryExclusive() {
    auto a = context(1, 0, 3);
    a.settleFamily = 7;
    Require(a.settleFamily != a.queueFamily && a.settleFamily != a.asyncFamily, "settle regression needs a third family");
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = 256;
    info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkBuffer buffer = VK_NULL_HANDLE;
    a.CreateBuffer(&info, nullptr, &buffer);
    Require(first.families == std::vector<std::uint32_t>{0, 3}, "normal buffer did not enable sharing before settle allocation");
    a.Function<PFN_vkCreateBuffer>("vkCreateBuffer")(a.device, &info, nullptr, &buffer);
    Require(first.original == &info && first.info.sharingMode == VK_SHARING_MODE_EXCLUSIVE && first.info.queueFamilyIndexCount == 0, "settle-only exclusive buffer was restricted to main and async families");
    VkExternalMemoryBufferCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO};
    external.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
    VkBufferCreateInfo alias{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, &external};
    alias.size = 65536;
    alias.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    a.Function<PFN_vkCreateBuffer>("vkCreateBuffer")(a.device, &alias, nullptr, &buffer);
    Require(first.original == &alias && first.info.sharingMode == VK_SHARING_MODE_EXCLUSIVE && first.info.queueFamilyIndexCount == 0 && first.info.pNext == &external, "settle-only host alias was converted by device-global sharing state");
    a.CreateBuffer(&info, nullptr, &buffer);
    Require(first.families == std::vector<std::uint32_t>{0, 3}, "settle-only allocation disabled later lane sharing");
}

void commandIsolation() {
    Context a{};
    a.asyncCommands = std::make_shared<AgcDriver::Graphics::AsyncCommandBuffers>();
    auto saved = a;
    const auto command = reinterpret_cast<VkCommandBuffer>(std::uintptr_t{1});
    const auto mainCommand = reinterpret_cast<VkCommandBuffer>(std::uintptr_t{2});
    a.asyncCommands->Insert(command);
    Require(saved.asyncCommands->Contains(command) && !saved.asyncCommands->Contains(mainCommand), "early context snapshot lost actual command ownership");
    {
        Context b{};
        b.asyncCommands = std::make_shared<AgcDriver::Graphics::AsyncCommandBuffers>();
        Require(!b.asyncCommands->Contains(command), "second device inherited command ownership");
        b.asyncCommands->Insert(mainCommand);
        Require(!a.asyncCommands->Contains(mainCommand), "second device changed the first command registry");
    }
    Context recreated{};
    recreated.asyncCommands = std::make_shared<AgcDriver::Graphics::AsyncCommandBuffers>();
    Require(!recreated.asyncCommands->Contains(mainCommand), "replacement device reused old command ownership");
    a.asyncCommands->Remove(command);
    Require(!saved.asyncCommands->Contains(command), "freed command handle remained async in an early context");
    Require(!recreated.asyncCommands->Contains(command), "command retirement changed the replacement device");
}

void barrierFiltering() {
    Context a{};
    a.asyncCommands = std::make_shared<AgcDriver::Graphics::AsyncCommandBuffers>();
    const auto asyncCommand = reinterpret_cast<VkCommandBuffer>(std::uintptr_t{11});
    const auto mainCommand = reinterpret_cast<VkCommandBuffer>(std::uintptr_t{12});
    a.asyncCommands->Insert(asyncCommand);
    constexpr VkPipelineStageFlags graphicsStages = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
        VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT |
        VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    constexpr VkAccessFlags graphicsAccess = VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
        VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    constexpr VkPipelineStageFlags legalSourceStages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT |
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    constexpr VkPipelineStageFlags legalDestinationStages = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_HOST_BIT |
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    constexpr VkAccessFlags legalSourceAccess = VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
        VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    constexpr VkAccessFlags legalDestinationAccess = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_HOST_READ_BIT |
        VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    const auto mixed = [&](VkCommandBuffer command, bool filtered) {
        VkPipelineStageFlags sourceStage = graphicsStages | legalSourceStages;
        VkPipelineStageFlags destinationStage = graphicsStages | legalDestinationStages;
        VkAccessFlags sourceAccess = graphicsAccess | legalSourceAccess;
        VkAccessFlags destinationAccess = graphicsAccess | legalDestinationAccess;
        AgcDriver::Graphics::FilterCommandBarrier(a, command, sourceStage, destinationStage, sourceAccess, destinationAccess);
        Require(sourceStage == (filtered ? legalSourceStages : graphicsStages | legalSourceStages) &&
            destinationStage == (filtered ? legalDestinationStages : graphicsStages | legalDestinationStages),
            "barrier stage filtering did not follow the actual command handle");
        Require(sourceAccess == (filtered ? legalSourceAccess : graphicsAccess | legalSourceAccess) &&
            destinationAccess == (filtered ? legalDestinationAccess : graphicsAccess | legalDestinationAccess),
            "barrier access filtering removed legal access or retained graphics-only access");
    };
    mixed(asyncCommand, true);
    mixed(mainCommand, false);
    VkPipelineStageFlags sourceStage = graphicsStages;
    VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkAccessFlags sourceAccess = graphicsAccess;
    VkAccessFlags destinationAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    AgcDriver::Graphics::FilterCommandBarrier(a, asyncCommand, sourceStage, destinationStage, sourceAccess, destinationAccess);
    Require(sourceStage == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT && destinationStage == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        "graphics-only async barrier left an empty stage mask");
    Require(sourceAccess == 0 && destinationAccess == 0, "graphics-only async barrier retained unsupported accesses");
    a.asyncCommands->Remove(asyncCommand);
    mixed(asyncCommand, false);
    a.asyncCommands.reset();
    mixed(mainCommand, false);
}

}

int main() {
    try {
        sharing();
        deviceIsolation();
        contextOverride();
        auxiliaryExclusive();
        commandIsolation();
        barrierFiltering();
        std::puts("async buffer sharing tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
