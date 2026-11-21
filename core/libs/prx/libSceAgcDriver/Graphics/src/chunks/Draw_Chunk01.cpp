#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libc/include/RunReport.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GpuColorTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexPlanScratch.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/StagedMerge.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SampleOrder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/UnitShadow.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/TracyZone.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileCounter.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadScratch.hpp"
#include "ThreadOwned.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Optimization/include/Optimization/ShaderStageInputInfo.hpp"
#include "SpirvBackend/SpirvAnalysis.hpp"
#include "ThreadOwned.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <functional>
#include <optional>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <set>
#include <unordered_map>
#include <type_traits>
#include <utility>

namespace AgcDriver::Graphics {

void WriteCheckpointMarker(const Context& context, VkCommandBuffer commands, std::uint64_t serial, std::uint32_t queue);
void WriteNvCheckpointToken(const Context& context, VkCommandBuffer commands, std::uint64_t token, std::uint32_t queue);

namespace {
enum class VkDrawKind : std::size_t { Direct, Indexed, Indirect, Mesh, Count };
std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(VkDrawKind::Count)> vkDrawCounts{};
void CountVkDraw(VkDrawKind kind) {
    vkDrawCounts[static_cast<std::size_t>(kind)].fetch_add(1, std::memory_order_relaxed);
}


bool DrawFaultTokens() {
    static const bool enabled = std::getenv("APS5_GPU_CHECKPOINTS") != nullptr;
    return enabled;
}

void noteDrawFaultToken(const Context& context, Recorder& recorder, VkCommandBuffer commands, std::span<const CompiledShader> shaders, const Pm4::DrawParameters& draw, std::uint32_t stage, const Pm4::DrawParameters::IndirectDraw* args) {
    try {
        if (!DrawFaultTokens()) return;
        const auto queue = GuestMemory::GpuLockThreadTag();
        if (queue != 0) return;
        const auto faultToken = AgcDriver::FaultTokenRing::Instance().Claim();
        if (faultToken == 0) return;
        if (auto* entry = AgcDriver::FaultTokenRing::Instance().Mutable(faultToken)) {
            entry->submitSerial = recorder.OpenSerial();
            entry->programAddress = 0;
            std::uint64_t variant = 0;
            for (const auto& shader : shaders) {
                if (shader.program != nullptr && (variant == 0 || shader.stage == ShaderRecompiler::ShaderStage::Fragment)) variant = shader.program->variantId;
            }
            entry->variantId = variant;
            entry->stage = stage;
            entry->argBuffer = 0;
            entry->argOffset = 0;
            entry->argStride = 0;
            entry->argCount = 0;
            for (auto& word : entry->argWords) word = 0;
            if (args != nullptr) {
                entry->argBuffer = args->arguments;
                entry->argOffset = 0;
                entry->argStride = args->stride;
                try {
                    constexpr std::uint32_t argWordLimit = 4;
                    const auto first = Pm4::ReadDrawArgumentsUnsynced(*args, 0);
                    entry->argWords[0] = first.count;
                    entry->argWords[1] = first.instances;
                    entry->argWords[2] = first.firstVertexOrIndex;
                    entry->argWords[3] = draw.indexed ? first.vertexOffset : first.firstInstance;
                    entry->argCount = argWordLimit;
                } catch (...) {
                    entry->argCount = 0;
                }
            }
            std::snprintf(entry->desc, sizeof(entry->desc), "draw%s%s c=%u i=%u", draw.indexed ? " indexed" : "", args != nullptr ? " indirect" : "", draw.indexCount, draw.instanceCount);
            recorder.NoteFaultToken(faultToken);
            std::atomic_ref<AgcDriver::FaultToken>(entry->token).store(faultToken, std::memory_order_release);
        }
        WriteCheckpointMarker(context, commands, faultToken, queue);
        WriteNvCheckpointToken(context, commands, faultToken, queue);
    } catch (...) {
    }
}

struct PixelFunctionLdsPool {
    Context context{};
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
    VkDescriptorSetLayout empty = VK_NULL_HANDLE;
    VkDescriptorSetLayout bufferLayout = VK_NULL_HANDLE;
    std::uint32_t* header = nullptr;
    std::uint32_t subgroupSize = 0;
    std::uint32_t slots = 0;
    bool announced = false;
};

std::mutex& PixelLdsMutex() {
    static std::mutex mutex;
    return mutex;
}

std::unordered_map<VkDevice, PixelFunctionLdsPool>& PixelLdsPools() {
    static std::unordered_map<VkDevice, PixelFunctionLdsPool> pools;
    return pools;
}

struct MeshWindowPool {
    Context context{};
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
};

std::mutex& MeshWindowMutex() {
    static std::mutex mutex;
    return mutex;
}

std::unordered_map<VkDevice, MeshWindowPool>& MeshWindowPools() {
    static std::unordered_map<VkDevice, MeshWindowPool> pools;
    return pools;
}

void DestroyMeshWindow(MeshWindowPool& pool) {
    if (pool.descriptorPool != VK_NULL_HANDLE) pool.context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(pool.context.device, pool.descriptorPool, nullptr);
    if (pool.buffer != VK_NULL_HANDLE) pool.context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(pool.context.device, pool.buffer, nullptr);
    if (pool.memory != VK_NULL_HANDLE) MemoryAccounting::Free(pool.context, pool.memory);
    if (pool.layout != VK_NULL_HANDLE) pool.context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(pool.context.device, pool.layout, nullptr);
    pool = MeshWindowPool{};
}

void DestroyPixelFunctionLds(PixelFunctionLdsPool& pool) {
    if (pool.header != nullptr) pool.context.Function<PFN_vkUnmapMemory>("vkUnmapMemory")(pool.context.device, pool.memory);
    if (pool.descriptorPool != VK_NULL_HANDLE) pool.context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(pool.context.device, pool.descriptorPool, nullptr);
    if (pool.buffer != VK_NULL_HANDLE) pool.context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(pool.context.device, pool.buffer, nullptr);
    if (pool.memory != VK_NULL_HANDLE) MemoryAccounting::Free(pool.context, pool.memory);
    if (pool.bufferLayout != VK_NULL_HANDLE) pool.context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(pool.context.device, pool.bufferLayout, nullptr);
    if (pool.empty != VK_NULL_HANDLE) pool.context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(pool.context.device, pool.empty, nullptr);
    pool.header = nullptr;
    pool.descriptorPool = VK_NULL_HANDLE;
    pool.set = VK_NULL_HANDLE;
    pool.buffer = VK_NULL_HANDLE;
    pool.memory = VK_NULL_HANDLE;
    pool.bufferLayout = VK_NULL_HANDLE;
    pool.empty = VK_NULL_HANDLE;
}

}

PixelFunctionLdsLayouts PixelFunctionLdsSetLayouts(const Context& context) {
    std::lock_guard lock(PixelLdsMutex());
    auto& pool = PixelLdsPools()[context.device];
    if (pool.bufferLayout == VK_NULL_HANDLE) {
        pool.context = context;
        VkDescriptorSetLayoutCreateInfo emptyInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &emptyInfo, nullptr, &pool.empty), "vkCreateDescriptorSetLayout pixel function LDS empty");
        const VkDescriptorSetLayoutBinding binding{0u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        info.bindingCount = 1u;
        info.pBindings = &binding;
        Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &info, nullptr, &pool.bufferLayout), "vkCreateDescriptorSetLayout pixel function LDS");
    }
    return {pool.empty, pool.bufferLayout};
}

void PreparePixelFunctionLds(const Context& context, VkCommandBuffer commands, std::uint32_t subgroupSize) {
    const auto operations = VK_SUBGROUP_FEATURE_BASIC_BIT | VK_SUBGROUP_FEATURE_BALLOT_BIT;
    Require((context.subgroup.supportedStages & VK_SHADER_STAGE_FRAGMENT_BIT) != 0 && (context.subgroup.supportedOperations & operations) == operations, "pixel function LDS needs fragment subgroup elect, ballot and broadcast");
    Require(subgroupSize != 0u && subgroupSize == context.subgroup.subgroupSize, "pixel function LDS subgroup size does not match the device");
    std::lock_guard lock(PixelLdsMutex());
    auto& pool = PixelLdsPools()[context.device];
    Require(pool.bufferLayout != VK_NULL_HANDLE, "pixel function LDS descriptor set layout was not created");
    if (pool.buffer == VK_NULL_HANDLE) {
        const auto stride = static_cast<std::uint64_t>(subgroupSize) * ShaderRecompiler::FunctionLdsDwordLimit * 4ull;
        const auto budget = std::min<std::uint64_t>(context.limits.maxStorageBufferRange, 2ull << 30);
        Require(budget > 16ull + stride, "pixel function LDS does not fit in the device storage buffer range");
        pool.slots = static_cast<std::uint32_t>((budget - 16ull) / stride);
        pool.subgroupSize = subgroupSize;
        const auto bytes = 16ull + stride * pool.slots;
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = bytes;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        Check(context.Function<PFN_vkCreateBuffer>("vkCreateBuffer")(context.device, &bufferInfo, nullptr, &pool.buffer), "vkCreateBuffer pixel function LDS");
        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(context.device, pool.buffer, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        constexpr auto hostCoherent = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        constexpr auto deviceLocalHost = hostCoherent | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        std::optional<std::uint32_t> deviceLocalType;
        std::optional<std::uint32_t> hostOnlyType;
        for (std::uint32_t i = 0; i < context.memory.memoryTypeCount; ++i) {
            if ((requirements.memoryTypeBits & (1u << i)) == 0) continue;
            const auto flags = context.memory.memoryTypes[i].propertyFlags;
            const auto heap = context.memory.memoryTypes[i].heapIndex;
            if (!deviceLocalType && (flags & deviceLocalHost) == deviceLocalHost && context.memory.memoryHeaps[heap].size >= requirements.size) {
                deviceLocalType = i;
            }
            if (!hostOnlyType && (flags & hostCoherent) == hostCoherent && (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == 0) {
                hostOnlyType = i;
            }
        }
        const auto hostType = hostOnlyType.value_or(context.MemoryType(requirements.memoryTypeBits, hostCoherent));
        allocation.memoryTypeIndex = deviceLocalType.value_or(hostType);
        auto allocated = MemoryAccounting::Allocate(context, allocation, "pixel function LDS", &pool.memory);
        if (allocated != VK_SUCCESS && deviceLocalType && allocation.memoryTypeIndex == *deviceLocalType && hostType != *deviceLocalType) {
            allocation.memoryTypeIndex = hostType;
            allocated = MemoryAccounting::Allocate(context, allocation, "pixel function LDS", &pool.memory);
        }
        if (allocated != VK_SUCCESS) {
            context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, pool.buffer, nullptr);
            pool.buffer = VK_NULL_HANDLE;
            pool.memory = VK_NULL_HANDLE;
            Check(allocated, "vkAllocateMemory pixel function LDS");
        }
        Check(context.Function<PFN_vkBindBufferMemory>("vkBindBufferMemory")(context.device, pool.buffer, pool.memory, 0), "vkBindBufferMemory pixel function LDS");
        Check(context.Function<PFN_vkMapMemory>("vkMapMemory")(context.device, pool.memory, 0, 16, 0, reinterpret_cast<void**>(&pool.header)), "vkMapMemory pixel function LDS");
        pool.header[0] = 0u;
        pool.header[1] = 0u;
        pool.header[2] = pool.slots;
        const VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.maxSets = 1u;
        poolInfo.poolSizeCount = 1u;
        poolInfo.pPoolSizes = &size;
        Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &poolInfo, nullptr, &pool.descriptorPool), "vkCreateDescriptorPool pixel function LDS");
        VkDescriptorSetAllocateInfo sets{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        sets.descriptorPool = pool.descriptorPool;
        sets.descriptorSetCount = 1u;
        sets.pSetLayouts = &pool.bufferLayout;
        Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &sets, &pool.set), "vkAllocateDescriptorSets pixel function LDS");
        const VkDescriptorBufferInfo buffer{pool.buffer, 0, bytes};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = pool.set;
        write.dstBinding = 0u;
        write.descriptorCount = 1u;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.pBufferInfo = &buffer;
        context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, 1u, &write, 0u, nullptr);
        std::fprintf(stderr, "[gpu] pixel function LDS pool: %u subgroups, %llu MiB, stride %u lanes\n", pool.slots, static_cast<unsigned long long>(bytes >> 20u), subgroupSize);
        pool.announced = true;
    }
    Require(pool.subgroupSize == subgroupSize, "pixel function LDS subgroup size does not match the pool");
    const VkMemoryBarrier beforeFill{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT};
    context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &beforeFill, 0, nullptr, 0, nullptr);
    context.Resolved(&DeviceFunctions::cmdFillBuffer, "vkCmdFillBuffer")(commands, pool.buffer, 0, 4, 0u);
    const VkMemoryBarrier afterFill{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT};
    context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &afterFill, 0, nullptr, 0, nullptr);
}

bool UsesMeshWindow(std::span<const CompiledShader> shaders) {
    return std::ranges::any_of(shaders, [](const CompiledShader& shader) { return shader.program != nullptr && shader.program->meshWindowBuffer; });
}

VkDescriptorSetLayout MeshWindowSetLayout(const Context& context) {
    std::lock_guard lock(MeshWindowMutex());
    auto& pool = MeshWindowPools()[context.device];
    if (pool.layout == VK_NULL_HANDLE) {
        pool.context = context;
        const VkDescriptorSetLayoutBinding binding{0u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_MESH_BIT_EXT, nullptr};
        VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        info.bindingCount = 1u;
        info.pBindings = &binding;
        Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &info, nullptr, &pool.layout), "vkCreateDescriptorSetLayout mesh window");
    }
    return pool.layout;
}

void PrepareMeshWindow(const Context& context) {
    std::lock_guard lock(MeshWindowMutex());
    auto& pool = MeshWindowPools()[context.device];
    Require(pool.layout != VK_NULL_HANDLE, "mesh window descriptor set layout was not created");
    if (pool.set != VK_NULL_HANDLE) return;
    try {
        const auto bytes = (static_cast<std::uint64_t>(ShaderRecompiler::MeshWindowHeaderDwords) + static_cast<std::uint64_t>(ShaderRecompiler::MeshWindowSlots) * ShaderRecompiler::MeshWindowSlotDwords) * 4ull;
        Require(bytes <= context.limits.maxStorageBufferRange, "mesh window pool does not fit in the device storage buffer range");
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = bytes;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        Check(context.Function<PFN_vkCreateBuffer>("vkCreateBuffer")(context.device, &bufferInfo, nullptr, &pool.buffer), "vkCreateBuffer mesh window");
        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(context.device, pool.buffer, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        Check(MemoryAccounting::Allocate(context, allocation, "mesh window", &pool.memory), "vkAllocateMemory mesh window");
        Check(context.Function<PFN_vkBindBufferMemory>("vkBindBufferMemory")(context.device, pool.buffer, pool.memory, 0), "vkBindBufferMemory mesh window");
        const VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.maxSets = 1u;
        poolInfo.poolSizeCount = 1u;
        poolInfo.pPoolSizes = &size;
        Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &poolInfo, nullptr, &pool.descriptorPool), "vkCreateDescriptorPool mesh window");
        VkDescriptorSetAllocateInfo sets{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        sets.descriptorPool = pool.descriptorPool;
        sets.descriptorSetCount = 1u;
        sets.pSetLayouts = &pool.layout;
        VkDescriptorSet set = VK_NULL_HANDLE;
        Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &sets, &set), "vkAllocateDescriptorSets mesh window");
        const VkDescriptorBufferInfo buffer{pool.buffer, 0, bytes};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = set;
        write.dstBinding = 0u;
        write.descriptorCount = 1u;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.pBufferInfo = &buffer;
        context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, 1u, &write, 0u, nullptr);
        CommandBatch batch(context);
        const auto commands = batch.Handle();
        context.Function<PFN_vkCmdFillBuffer>("vkCmdFillBuffer")(commands, pool.buffer, 0, ShaderRecompiler::MeshWindowHeaderDwords * 4u, 0u);
        const VkMemoryBarrier afterFill{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT};
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &afterFill, 0, nullptr, 0, nullptr);
        batch.SubmitAndWait();
        pool.set = set;
        std::fprintf(stderr, "[gpu] mesh window pool: %u workgroup slots, %llu MiB\n", ShaderRecompiler::MeshWindowSlots, static_cast<unsigned long long>(bytes >> 20u));
    } catch (...) {
        const auto layout = pool.layout;
        pool.layout = VK_NULL_HANDLE;
        DestroyMeshWindow(pool);
        pool.context = context;
        pool.layout = layout;
        throw;
    }
}

void BindMeshWindow(const Context& context, VkCommandBuffer commands, VkPipelineLayout layout) {
    std::lock_guard lock(MeshWindowMutex());
    const auto& pool = MeshWindowPools().at(context.device);
    Require(pool.set != VK_NULL_HANDLE, "mesh window buffer was not prepared");
    context.Resolved(&DeviceFunctions::cmdBindDescriptorSets, "vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, ShaderRecompiler::MeshWindowDescriptorSet, 1, &pool.set, 0, nullptr);
}

void ReleaseMeshWindow(VkDevice device) {
    std::lock_guard lock(MeshWindowMutex());
    const auto found = MeshWindowPools().find(device);
    if (found == MeshWindowPools().end()) return;
    DestroyMeshWindow(found->second);
    MeshWindowPools().erase(found);
}

std::uint32_t PixelFunctionLdsSubgroup(std::span<const CompiledShader> shaders) {
    std::uint32_t subgroup = 0u;
    for (const auto& shader : shaders) {
        if (shader.program != nullptr && shader.program->functionLdsBuffer) subgroup = shader.program->hostSubgroupSize;
    }
    return subgroup;
}

void BindPixelFunctionLds(const Context& context, VkCommandBuffer commands, VkPipelineLayout layout) {
    std::lock_guard lock(PixelLdsMutex());
    const auto& pool = PixelLdsPools().at(context.device);
    Require(pool.set != VK_NULL_HANDLE, "pixel function LDS buffer was not prepared");
    context.Resolved(&DeviceFunctions::cmdBindDescriptorSets, "vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, ShaderRecompiler::PixelFunctionLdsDescriptorSet, 1, &pool.set, 0, nullptr);
}

void CheckPixelFunctionLds(const Context& context) {
    std::lock_guard lock(PixelLdsMutex());
    const auto found = PixelLdsPools().find(context.device);
    if (found == PixelLdsPools().end() || found->second.header == nullptr) return;
    const auto exhausted = found->second.header[1] != 0u;
    found->second.header[1] = 0u;
    if (exhausted) throw std::runtime_error("AGC graphics: pixel function LDS subgroup slots exhausted");
}

void ReleasePixelFunctionLds(VkDevice device) {
    std::lock_guard lock(PixelLdsMutex());
    const auto found = PixelLdsPools().find(device);
    if (found == PixelLdsPools().end()) return;
    DestroyPixelFunctionLds(found->second);
    PixelLdsPools().erase(found);
}

std::uint64_t DrawRenderPassKey(const Context& context, const State& state, std::span<const VkImageView> targetViews) {
    std::uint64_t key = 14695981039346656037ull;
    const auto mix = [&](std::uint64_t value) {
        key ^= value;
        key *= 1099511628211ull;
    };
    for (const auto view : targetViews) mix(reinterpret_cast<std::uint64_t>(view));
    if (state.blends.size() != state.colors.size()) {
        mix(state.blends.size());
        for (const auto& color : state.colors) mix(color.exportIndex);
    }
    for (const auto& color : state.colors) {
        if (color.partialWriteMask != 0u) mix(0x6d65726765000000ull | color.exportIndex);
    }
    mix(state.renderExtent.width);
    mix(state.renderExtent.height);
    mix(state.samples);
    mix(state.mergedCoverage);
    mix(state.truncatedCoverage);
    mix(state.customSampleLocations);
    if (state.customSampleLocations) {
        mix(state.sampleLocationGrid);
        for (const auto location : state.sampleLocations) mix(location);
    }
    if (!context.provokingVertexModePerPipeline) mix(state.provokingVertexMode);
    if (state.rasterizerDiscard) mix(0x6469736361726400ull);
    return key;
}

namespace {

bool writesStencilTarget(const State& state) {
    const auto keeps = [](const VkStencilOpState& face) {
        return face.writeMask == 0u || (face.failOp == VK_STENCIL_OP_KEEP && face.passOp == VK_STENCIL_OP_KEEP && face.depthFailOp == VK_STENCIL_OP_KEEP);
    };
    return state.stencilTest && (!keeps(state.stencilFront) || !keeps(state.stencilBack));
}

bool writesDepthTarget(const State& state) {
    return state.depthWrite || writesStencilTarget(state);
}

std::optional<std::uint8_t> overwrittenStencil(const State& state) {
    const auto replaces = [](const VkStencilOpState& face) { return face.compareOp == VK_COMPARE_OP_ALWAYS && face.passOp == VK_STENCIL_OP_REPLACE && face.writeMask == 0xffu; };
    if (!state.depthTest || !state.depthWrite || state.depthCompare != VK_COMPARE_OP_ALWAYS || state.depthBoundsTest || !state.stencilTest) return std::nullopt;
    if (!replaces(state.stencilFront) || !replaces(state.stencilBack) || state.stencilFront.reference != state.stencilBack.reference) return std::nullopt;
    return static_cast<std::uint8_t>(state.stencilFront.reference);
}

std::size_t colorKeyCount(const ColorTarget& color, std::size_t bytes) {
    if (color.mipCount != 1 || color.depth != 1 || !color.dccPipeAligned) return DccKeyBytes(bytes);
    return DccKeyCount(ColorTextureTileMode(color.tileMode), color.elementBytes, color.extent.width, color.extent.height, bytes);
}

}
}
