#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_CONTEXT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_CONTEXT_HPP

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <mutex>
#include <vulkan/vulkan.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace AgcDriver::Graphics {


class TextureDetiler;
class GpuColorTransfer;
class BufferPool;
class TextureCache;
class RenderCache;
class DrawQueue;
class GraphicsPipelineCache;
class Recorder;
class DescriptorCache;
class SamplerCache;
class ShaderResources;
class PlaceholderImages;
class VertexInputCache;

class PipelineCacheSynchronization {
public:
    using PipelineUseLock = std::shared_lock<std::shared_mutex>;
    using SnapshotLock = std::unique_lock<std::shared_mutex>;

    PipelineUseLock LockForPipelineUse() const { return PipelineUseLock(mutex); }
    SnapshotLock LockForSnapshot() const { return SnapshotLock(mutex); }
    SnapshotLock TryLockForSnapshot() const { return SnapshotLock(mutex, std::try_to_lock); }

private:
    mutable std::shared_mutex mutex;
};

inline void Require(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error("AGC graphics: " + reason);
}

inline std::function<void()>& DeviceLostHook() {
    static std::function<void()> hook;
    return hook;
}

inline const void*& DeviceLostOwner() {
    static const void* owner = nullptr;
    return owner;
}

inline void ReportDeviceLost() {
    static std::atomic<bool> reported{false};
    if (DeviceLostHook() && !reported.exchange(true)) DeviceLostHook()();
}

inline void Check(VkResult result, const char* operation) {
    if (result == VK_ERROR_DEVICE_LOST) ReportDeviceLost();
    if (result != VK_SUCCESS) throw std::runtime_error(std::string("AGC graphics: ") + operation + ": Vulkan result " + std::to_string(result));
}

inline VkShaderModule CreateShaderModule(PFN_vkCreateShaderModule create, VkDevice device, bool graphicsPipelineLibrary, const std::uint32_t* code, std::size_t bytes, const char* operation) {
    std::vector<std::uint32_t> keyed(code, code + bytes / sizeof(std::uint32_t));
    if (!graphicsPipelineLibrary && keyed.size() > 2) keyed[2] ^= 0x8000u;
    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize = keyed.size() * sizeof(std::uint32_t);
    info.pCode = keyed.data();
    VkShaderModule module = VK_NULL_HANDLE;
    Check(create(device, &info, nullptr, &module), operation);
    return module;
}

inline void Require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(std::string("AGC graphics: ") + reason);
}

struct DescriptorRejected : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct BdaUnalignedAccess : std::runtime_error {
    BdaUnalignedAccess(const std::string& message, std::uint32_t instruction) : std::runtime_error(message), instruction(instruction) {}
    std::uint32_t instruction;
};

// Device entry points resolved once per device (VulkanDevice's State fills it after setup): the
// loader's vkGetDeviceProcAddr is a name lookup under its global mutex per call, paid at every
// record site otherwise. Null members (tests, APS5_NO_PROC_TABLE=1) resolve per call (Resolved).
struct DeviceFunctions {
    PFN_vkCmdPipelineBarrier cmdPipelineBarrier = nullptr;
    PFN_vkCmdCopyBuffer cmdCopyBuffer = nullptr;
    PFN_vkCmdUpdateBuffer cmdUpdateBuffer = nullptr;
    PFN_vkCmdFillBuffer cmdFillBuffer = nullptr;
    PFN_vkCmdBindPipeline cmdBindPipeline = nullptr;
    PFN_vkCmdBindDescriptorSets cmdBindDescriptorSets = nullptr;
    PFN_vkCmdPushConstants cmdPushConstants = nullptr;
    PFN_vkCmdDispatch cmdDispatch = nullptr;
    PFN_vkCmdDispatchIndirect cmdDispatchIndirect = nullptr;
    PFN_vkCmdBeginRenderPass cmdBeginRenderPass = nullptr;
    PFN_vkCmdEndRenderPass cmdEndRenderPass = nullptr;
    PFN_vkCmdBeginRenderingKHR cmdBeginRendering = nullptr;
    PFN_vkCmdEndRenderingKHR cmdEndRendering = nullptr;
    PFN_vkCmdSetViewport cmdSetViewport = nullptr;
    PFN_vkCmdSetScissor cmdSetScissor = nullptr;
    PFN_vkCmdSetDepthBounds cmdSetDepthBounds = nullptr;
    PFN_vkCmdSetDepthBias cmdSetDepthBias = nullptr;
    PFN_vkCmdSetCullModeEXT cmdSetCullMode = nullptr;
    PFN_vkCmdSetFrontFaceEXT cmdSetFrontFace = nullptr;
    PFN_vkCmdSetDepthTestEnableEXT cmdSetDepthTestEnable = nullptr;
    PFN_vkCmdSetDepthWriteEnableEXT cmdSetDepthWriteEnable = nullptr;
    PFN_vkCmdSetDepthCompareOpEXT cmdSetDepthCompareOp = nullptr;
    PFN_vkCmdSetDepthBoundsTestEnableEXT cmdSetDepthBoundsTestEnable = nullptr;
    PFN_vkCmdSetStencilTestEnableEXT cmdSetStencilTestEnable = nullptr;
    PFN_vkCmdSetStencilOpEXT cmdSetStencilOp = nullptr;
    PFN_vkCmdSetStencilCompareMask cmdSetStencilCompareMask = nullptr;
    PFN_vkCmdSetStencilWriteMask cmdSetStencilWriteMask = nullptr;
    PFN_vkCmdSetStencilReference cmdSetStencilReference = nullptr;
    PFN_vkCmdBindVertexBuffers cmdBindVertexBuffers = nullptr;
    PFN_vkCmdBindIndexBuffer cmdBindIndexBuffer = nullptr;
    PFN_vkCmdDraw cmdDraw = nullptr;
    PFN_vkCmdDrawIndexed cmdDrawIndexed = nullptr;
    PFN_vkCmdDrawIndirect cmdDrawIndirect = nullptr;
    PFN_vkCmdDrawIndexedIndirect cmdDrawIndexedIndirect = nullptr;
    PFN_vkCmdCopyBufferToImage cmdCopyBufferToImage = nullptr;
    PFN_vkCmdCopyImageToBuffer cmdCopyImageToBuffer = nullptr;
    PFN_vkCmdClearColorImage cmdClearColorImage = nullptr;
    PFN_vkUpdateDescriptorSets updateDescriptorSets = nullptr;
    PFN_vkAllocateDescriptorSets allocateDescriptorSets = nullptr;
    PFN_vkGetFenceStatus getFenceStatus = nullptr;
};

// Per-thread count of vkGetDeviceProcAddr lookups made through Context::Function (the [vk] line).
inline std::uint64_t& DeviceProcLookups() {
    thread_local std::uint64_t count = 0;
    return count;
}

inline constexpr std::size_t EmptyBufferBytes = 16;

struct Context {
    VkDevice device;
    VkPhysicalDevice physical;
    VkQueue queue;
    VkCommandPool pool;
    PFN_vkGetDeviceProcAddr deviceProc;
    PFN_vkGetPhysicalDeviceFormatProperties formatProperties;
    PFN_vkGetPhysicalDeviceImageFormatProperties imageFormatProperties;
    VkPhysicalDeviceMemoryProperties memory;
    VkPhysicalDeviceLimits limits;
    bool tessellationShader = false;
    bool meshShader = false;
    VkPhysicalDeviceMeshShaderPropertiesEXT meshLimits{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT};
    bool depthClipControl = false;
    bool depthRangeUnrestricted = false;
    bool bufferDeviceAddress = false;
    VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    bool fragmentShaderBarycentric = false;
    bool samplerAnisotropy = false;
    bool textureCompressionBC = false;
    TextureDetiler* detiler = nullptr;
    GpuColorTransfer* colorTransfer = nullptr;
    mutable std::shared_ptr<BufferPool> bufferPool;
    TextureCache* textureCache = nullptr;
    VkPipelineCache pipelineCache = VK_NULL_HANDLE;
    bool depthClamp = false;
    // Nonzero when VK_EXT_external_memory_host is enabled: the required host pointer alignment.
    VkDeviceSize hostImportAlignment = 0;
    bool drawInputsInPlace = true;
    bool discreteGpu = false;
    bool dmaBufImport = false;
    bool sparseBinding = false;
    bool sparseResidencyBuffer = false;
    bool sparseResidencyAliased = false;
    std::uint32_t queueFamily = 0;
    std::uint32_t sparseFamily = VK_QUEUE_FAMILY_IGNORED;
    VkQueue sparseQueue = VK_NULL_HANDLE;
    std::mutex* sparseQueueMutex = nullptr;
    VkQueue settleQueue = VK_NULL_HANDLE;
    VkCommandPool settlePool = VK_NULL_HANDLE;
    std::uint32_t settleFamily = 0;
    std::mutex* settleQueueMutex = nullptr;
    PFN_vkGetPhysicalDeviceMemoryProperties2 memoryProperties2 = nullptr;
    VkDeviceSize (*cacheReclaim)(VkDeviceSize bytes, bool newImage) = nullptr;
    RenderCache* renderCache = nullptr;
    DrawQueue* drawQueue = nullptr;
    GraphicsPipelineCache* graphicsPipelines = nullptr;
    // Batches GPU work across guest commands (see Recorder); null before the device finished setup.
    Recorder* recorder = nullptr;
    // Per-device caches of the immutable descriptor objects a ShaderResources build needs (set
    // layouts, descriptor pools, samplers); null (tests) means every build makes and destroys its own.
    DescriptorCache* descriptorCache = nullptr;
    SamplerCache* samplerCache = nullptr;
    // Indirect draw features of the device: records with a non-zero first instance, several records
    // per call, and a GPU-side draw count (VK_KHR_draw_indirect_count).
    bool drawIndirectFirstInstance = false;
    bool multiDrawIndirect = false;
    bool drawIndirectCount = false;
    bool occlusionQueryPrecise = false;
    bool depthBounds = false;
    bool depthBiasClamp = false;
    bool dualSrcBlend = false;
    bool samplerFilterMinmax = false;
    bool nonSeamlessCubeMap = false;
    VkSampleCountFlags mergedCoverageSampleCounts = 0;
    VkSampleCountFlags truncatedCoverageSampleCounts = 0;
    bool mixedAttachmentSamples = false;
    bool conservativeRasterization = false;
    bool meshWave32 = false;
    VkBuffer emptyBuffer = VK_NULL_HANDLE;
    // The device's list of recorded dispatches whose copied written buffers await a CPU write-back
    // (VulkanDevice's State::copiedWriters; the draw counterpart is DrawCopiedWriters): an indirect
    // draw whose records one of them produces reads them on the CPU. Null in tests.
    const std::vector<std::shared_ptr<ShaderResources>>* copiedWriters = nullptr;
    // The device's resolved entry points (see DeviceFunctions); null until the device set up.
    const DeviceFunctions* functions = nullptr;
    // VK_EXT_descriptor_indexing with non-uniform sampled/storage image array indexing enabled
    // (bindless image tables in graphics stages).
    bool descriptorIndexing = false;
    VkPhysicalDeviceDescriptorIndexingPropertiesEXT descriptorIndexingLimits{};
    bool imageInt64Atomics = false;
    bool runtimeDescriptorArray = false;
    bool partiallyBound = false;
    bool variableDescriptorCount = false;
    bool sampledImageUpdateAfterBind = false;
    std::uint32_t imageTableCapacity = 0;
    std::uint32_t samplerTableCapacity = 0;
    bool individuallyFreeDescriptorSets = false;
    bool imageTableUpdateAfterBind = false;
    bool samplerTableUpdateAfterBind = false;
    bool geometryShader = false;
    bool largePoints = false;
    bool sampleRateShading = false;
    bool fillModeNonSolid = false;
    bool wideLines = false;
    bool maintenance5 = false;
    bool storageImageMultisample = false;
    bool storageImageWriteWithoutFormat = false;
    bool imageFormatList = false;
    bool shaderClipDistance = false;
    bool shaderCullDistance = false;
    bool nullDescriptors = false;
    bool bufferInt64Atomics = false;
    const PlaceholderImages* placeholderImages = nullptr;
    bool viewportIndexLayer = false;
    bool demoteToHelperInvocation = false;
    bool earlyFragmentSampleMaskTestBeforeSampleCounting = false;
    bool primitiveListRestart = false;
    bool imageViewMinLod = false;
    bool pipelineExecutableInfo = false;
    bool graphicsPipelineLibrary = false;
    bool pipelineLinkTimeOptimization = true;
    std::uint32_t srgbDecodeFormats = 0;
    std::uint32_t srgbCopyFormats = 0;
    bool provokingVertexLast = false;
    bool provokingVertexModePerPipeline = false;
    bool singlePassStorage = false;
    VkSampleCountFlags sampleLocationSampleCounts = 0;
    VkSampleCountFlags sampleLocationGridSampleCounts = 0;
    std::array<VkExtent2D, 4> sampleLocationGridSizes{};
    bool shaderStencilExport = false;
    PipelineCacheSynchronization* pipelineCacheSynchronization = nullptr;
    bool shaderTerminateInvocation = false;

    PipelineCacheSynchronization::PipelineUseLock LockPipelineCacheForUse() const {
        return pipelineCacheSynchronization != nullptr ? pipelineCacheSynchronization->LockForPipelineUse() : PipelineCacheSynchronization::PipelineUseLock{};
    }
    VertexInputCache* vertexInputs = nullptr;

    template<typename TFunction>
    TFunction Function(const char* name) const {
        Require(deviceProc != nullptr, "missing Vulkan device function resolver");
        ++DeviceProcLookups();
        const auto function = reinterpret_cast<TFunction>(deviceProc(device, name));
        if (function == nullptr) throw std::runtime_error(std::string("AGC graphics: missing Vulkan function: ") + name);
        return function;
    }

    // The table's entry point, or a per-call lookup while the table is absent or lacks it.
    template<typename TFunction>
    TFunction Resolved(TFunction DeviceFunctions::*member, const char* name) const {
        if (functions != nullptr && functions->*member != nullptr) return functions->*member;
        return Function<TFunction>(name);
    }

    std::uint32_t MemoryType(std::uint32_t mask, VkMemoryPropertyFlags flags) const {
        for (std::uint32_t i = 0; i < memory.memoryTypeCount; ++i) {
            if ((mask & (1u << i)) != 0 && (memory.memoryTypes[i].propertyFlags & flags) == flags) return i;
        }
        throw std::runtime_error("AGC graphics: required Vulkan memory type is unavailable");
    }

    std::uint32_t BufferMemoryType(std::uint32_t mask, VkMemoryPropertyFlags flags, VkDeviceSize bytes) const {
        constexpr VkMemoryPropertyFlags coherent = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        auto type = MemoryType(mask, flags);
        if (flags == coherent) {
            for (std::uint32_t i = 0; i < memory.memoryTypeCount; ++i) {
                if ((mask & (1u << i)) != 0 && (memory.memoryTypes[i].propertyFlags & (coherent | VK_MEMORY_PROPERTY_HOST_CACHED_BIT)) == (coherent | VK_MEMORY_PROPERTY_HOST_CACHED_BIT)) {
                    type = i;
                    break;
                }
            }
        }
        if ((flags & coherent) != coherent || memoryProperties2 == nullptr || physical == VK_NULL_HANDLE) return type;
        VkPhysicalDeviceMemoryBudgetPropertiesEXT reported{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
        VkPhysicalDeviceMemoryProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2, &reported};
        memoryProperties2(physical, &properties);
        const auto heap = memory.memoryTypes[type].heapIndex;
        const auto fits = [&](std::uint32_t candidate) {
            return reported.heapBudget[candidate] != 0 && reported.heapUsage[candidate] < reported.heapBudget[candidate] && bytes <= reported.heapBudget[candidate] - reported.heapUsage[candidate];
        };
        if (reported.heapBudget[heap] == 0 || fits(heap)) return type;
        for (std::uint32_t i = 0; i < memory.memoryTypeCount; ++i) {
            const auto& candidate = memory.memoryTypes[i];
            if ((mask & (1u << i)) != 0 && (candidate.propertyFlags & flags) == flags && candidate.heapIndex != heap && fits(candidate.heapIndex)) return i;
        }
        return type;
    }

    std::uint32_t DeviceMemoryType(std::uint32_t mask, VkDeviceSize bytes, const char* what) const {
        const auto type = MemoryType(mask, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memoryProperties2 == nullptr || physical == VK_NULL_HANDLE) return type;
        VkPhysicalDeviceMemoryBudgetPropertiesEXT reported{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
        VkPhysicalDeviceMemoryProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
        properties.pNext = &reported;
        memoryProperties2(physical, &properties);
        const auto heap = memory.memoryTypes[type].heapIndex;
        const auto budget = reported.heapBudget[heap];
        const auto usage = reported.heapUsage[heap];
        if (budget == 0 || (usage < budget && bytes <= budget - usage)) return type;
        const auto host = HostMemoryType(mask);
        if (!host) return type;
        static std::atomic<std::uint64_t> placements{0}, placedBytes{0};
        const auto count = placements.fetch_add(1) + 1;
        const auto total = placedBytes.fetch_add(bytes) + bytes;
        if (count <= 5 || (count & (count - 1)) == 0) std::fprintf(stderr, "[gpu] %s: device-local heap %u uses %llu of its %llu MiB budget, %llu bytes placed in host memory type %u; %llu such allocations, %llu MiB\n", what, heap, static_cast<unsigned long long>(usage >> 20u), static_cast<unsigned long long>(budget >> 20u), static_cast<unsigned long long>(bytes), *host, static_cast<unsigned long long>(count), static_cast<unsigned long long>(total >> 20u));
        return *host;
    }

    std::optional<std::uint32_t> HostMemoryType(std::uint32_t mask) const {
        for (std::uint32_t i = 0; i < memory.memoryTypeCount; ++i) {
            if ((mask & (1u << i)) != 0 && (memory.memoryHeaps[memory.memoryTypes[i].heapIndex].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) == 0) return i;
        }
        return std::nullopt;
    }

    std::optional<std::uint32_t> RefusedDeviceMemoryFallback(std::uint32_t mask, std::uint32_t refused) const {
        if (refused >= memory.memoryTypeCount || (memory.memoryHeaps[memory.memoryTypes[refused].heapIndex].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) == 0) return std::nullopt;
        return HostMemoryType(mask);
    }
};

inline VkShaderModule CreateShaderModule(const Context& context, const std::uint32_t* code, std::size_t bytes, const char* operation) {
    return CreateShaderModule(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule"), context.device, context.graphicsPipelineLibrary, code, bytes, operation);
}

inline void EndRenderPass(const Context& context, VkCommandBuffer commands) {
    if (context.graphicsPipelineLibrary) context.Resolved(&DeviceFunctions::cmdEndRendering, "vkCmdEndRenderingKHR")(commands);
    else context.Resolved(&DeviceFunctions::cmdEndRenderPass, "vkCmdEndRenderPass")(commands);
}

std::uint32_t ImageTableCapacity(const Context& context);
std::uint32_t StorageTableCapacity(const Context& context);
std::uint32_t SamplerTableCapacity(const Context& context);
bool ImageTableUpdateAfterBind(const Context& context);
bool SamplerTableUpdateAfterBind(const Context& context);

}

#endif
