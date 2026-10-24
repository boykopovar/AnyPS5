#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/FrameDumpIo.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineSpecialization.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "BdaAbi.hpp"
#if ANYPS5_ENABLE_SPIRV_TOOLS
#include "SpirvBackend/SpirvOptimizer.hpp"
#endif
#include "prx/libSceAgcDriver/Graphics/include/PipelineJournal.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/FloatFlushProbe.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/BdaFeatures.hpp"
#include "prx/libSceAgcDriver/Execution/include/DeviceFault.hpp"
#include "prx/libSceAgcDriver/Execution/include/FragmentBarycentric.hpp"
#include "prx/libSceAgcDriver/Execution/include/Float32FlushControl.hpp"
#include "prx/libSceAgcDriver/Execution/include/PipelineLinkOptimization.hpp"
#include "prx/libSceAgcDriver/Execution/include/DrawInputsInPlace.hpp"
#include "prx/libSceAgcDriver/Execution/include/SubgroupBarrier.hpp"
#include "prx/libSceAgcDriver/Execution/include/SubgroupClock.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderDeviceProfile.hpp"
#include "prx/libSceAgcDriver/Execution/include/PresentMode.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceOverlay.hpp"
#include "prx/libSceAgcDriver/Execution/include/PresentationPass.hpp"
#include "prx/libSceAgcDriver/Execution/include/PresentationScaler.hpp"
#include "prx/libSceAgcDriver/Execution/include/Screenshot.hpp"
#include "prx/libkernel/AppMetadata/include/AppMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PlaceholderImages.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Execution/include/DisplayFormat.hpp"
#include "prx/libSceAgcDriver/Execution/include/HostHdr.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SampleOrder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GpuColorTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/General.hpp"
#ifdef _WIN32
#include <windows.h>
#endif
#include <atomic>
#include <chrono>
#include <charconv>
#include <condition_variable>
#include <future>
#include <fstream>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <mutex>
#include <SDL_loadso.h>
#include <SDL_error.h>
#include <spirv/unified1/spirv.hpp>
#include <array>
#include <optional>
#include <algorithm>
#include <bit>
#include <cstring>
#include <deque>
#include <limits>
#include <list>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>
#include <utility>

namespace AgcDriver {

DeviceQueuePlan PlanDeviceQueues(std::span<const VkQueueFamilyProperties> families, std::uint32_t family, bool settles, bool sparseBinding) {
    DeviceQueuePlan plan;
    const auto count = static_cast<std::uint32_t>(families.size());
    if (settles) {
        if (family < count && families[family].queueCount >= 2) {
            plan.mainCount = 2;
            plan.settle = true;
            plan.settleFamily = family;
            plan.settleIndex = 1;
        } else {
            const auto copies = [&](std::uint32_t index) { return index != family && families[index].queueCount != 0 && (families[index].queueFlags & (VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT)) != 0; };
            for (std::uint32_t i = 0; i < count && !plan.settle; ++i) {
                if (copies(i) && (families[i].queueFlags & (VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT)) == 0) plan.settle = true, plan.settleFamily = i;
            }
            for (std::uint32_t i = 0; i < count && !plan.settle; ++i) {
                if (copies(i)) plan.settle = true, plan.settleFamily = i;
            }
        }
    }
    if (!sparseBinding) return plan;
    const auto taken = [&](std::uint32_t index) { return plan.settle && index == plan.settleFamily ? plan.settleIndex + 1 : 0u; };
    const auto binds = [&](std::uint32_t index) { return index != family && families[index].queueCount > taken(index) && (families[index].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) != 0; };
    for (std::uint32_t i = 0; i < count && plan.sparseFamily == VK_QUEUE_FAMILY_IGNORED; ++i) {
        if (binds(i) && families[i].queueFlags == VK_QUEUE_SPARSE_BINDING_BIT) plan.sparseFamily = i;
    }
    for (std::uint32_t i = 0; i < count && plan.sparseFamily == VK_QUEUE_FAMILY_IGNORED; ++i) {
        if (binds(i)) plan.sparseFamily = i;
    }
    if (plan.sparseFamily != VK_QUEUE_FAMILY_IGNORED) {
        plan.sparseIndex = taken(plan.sparseFamily);
        plan.sparseBinding = true;
    } else {
        plan.sparseBinding = family < count && (families[family].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) != 0;
    }
    return plan;
}

namespace {

void check(VkResult result, const char* operation) {
    if (result == VK_ERROR_DEVICE_LOST) Graphics::ReportDeviceLost();
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string(operation) + ": Vulkan result " + std::to_string(result));
    }
}

void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(std::string("Vulkan presentation: ") + reason);
}

bool CoreVulkanCapability(std::uint32_t capability) {
    switch (capability) {
        case spv::CapabilityMatrix:
        case spv::CapabilityShader:
        case spv::CapabilityInputAttachment:
        case spv::CapabilitySampled1D:
        case spv::CapabilityImage1D:
        case spv::CapabilitySampledBuffer:
        case spv::CapabilityImageBuffer:
        case spv::CapabilityImageQuery:
        case spv::CapabilityDerivativeControl:
        case spv::CapabilityStorageImageExtendedFormats:
            return true;
        default:
            return false;
    }
}

void CreateComputePipeline(VkDevice device, PFN_vkCreateComputePipelines create, VkPipelineCache cache, VkShaderModule module, VkPipelineLayout layout, VkPipelineCreateFlags flags, VkPipelineShaderStageCreateFlags stageFlags, const VkSpecializationInfo* specialization, std::uint32_t subgroupSize, VkPipeline* pipeline, Graphics::PipelineCacheSynchronization* synchronization = nullptr) {
    VkComputePipelineCreateInfo info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    info.flags = flags;
    info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage.flags = stageFlags;
    info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    info.stage.module = module;
    info.stage.pName = "main";
    info.stage.pSpecializationInfo = specialization;
    VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT subgroup{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT};
    subgroup.requiredSubgroupSize = subgroupSize;
    if (subgroupSize != 0) info.stage.pNext = &subgroup;
    info.layout = layout;
    auto cacheLock = synchronization != nullptr ? synchronization->LockForPipelineUse() : Graphics::PipelineCacheSynchronization::PipelineUseLock{};
    check(create(device, cache, 1, &info, nullptr, pipeline), "vkCreateComputePipelines");
    PerformanceCounters::CountPipeline();
}

#ifdef _WIN32
const wchar_t* ComputePipelineDumpDirectory() { return _wgetenv(L"APS5_DUMP_COMPUTE_PIPELINES"); }
#else
const char* ComputePipelineDumpDirectory() { return std::getenv("APS5_DUMP_COMPUTE_PIPELINES"); }
#endif

bool CaptureComputePipeline(std::size_t words) {
    const char* minimum = std::getenv("APS5_DUMP_COMPUTE_MIN_WORDS");
    if (minimum == nullptr) return true;
    std::uint32_t threshold = 0;
    const auto end = minimum + std::strlen(minimum);
    const auto parsed = std::from_chars(minimum, end, threshold);
    require(parsed.ec == std::errc{} && parsed.ptr == end && threshold <= ComputePipelineCapture::MaxSpirvWords, "APS5_DUMP_COMPUTE_MIN_WORDS must be an integer from 0 through 8388608");
    return words >= threshold;
}

// The ShaderResources content cache dispatches share with recorded draws: Graphics::ResourceCache,
// one process-wide instance (see SharedResourceCache) that the State references so this file keeps
// its Find/Insert/Remove/Clear calls; the device clears it at teardown before its descriptor caches
// go. APS5_NO_RESOURCE_CACHE=1 builds every dispatch's resources as before.
using ResourceCache = Graphics::ResourceCache;

std::uint32_t DumpScale();
void WriteFrameBmp(int index, std::uint32_t fullWidth, std::uint32_t fullHeight, std::span<const std::byte> full, std::uint32_t scale);
FrameDumpOutput frameDumpOutput;

class FrameDumpWriter {
public:
    ~FrameDumpWriter() {
        {
            std::lock_guard lock(mutex);
            stopping = true;
        }
        changed.notify_all();
        if (worker.joinable()) worker.join();
    }

    void Enqueue(int index, std::uint32_t width, std::uint32_t height, std::uint32_t scale, bool screenshot, std::vector<std::byte> pixels) {
        std::unique_lock lock(mutex);
        require(!stopping, "frame dump writer has stopped");
        if (!screenshot && !frameDumpOutput.Enabled_nid_no_patch()) return;
        require(pixels.size() <= maxBytes, "a frame dump exceeds the 64 MiB queue");
        changed.wait(lock, [&] { return stopping || pixels.size() <= maxBytes - queuedBytes; });
        require(!stopping, "frame dump writer has stopped");
        if (!worker.joinable()) worker = std::thread([this] { run(); });
        const auto bytes = pixels.size();
        pending.push_back({index, width, height, scale, screenshot, std::move(pixels)});
        queuedBytes += bytes;
        changed.notify_all();
    }

    bool Idle() {
        std::lock_guard lock(mutex);
        return queuedBytes == 0;
    }

private:
    struct Frame {
        int index;
        std::uint32_t width;
        std::uint32_t height;
        std::uint32_t scale;
        bool screenshot;
        std::vector<std::byte> pixels;
    };

    void run() {
        try {
            for (;;) {
                std::deque<Frame> batch;
                {
                    std::unique_lock lock(mutex);
                    changed.wait(lock, [this] { return stopping || !pending.empty(); });
                    if (pending.empty() && stopping) return;
                    batch.swap(pending);
                }
                for (const auto& frame : batch) {
                    if (frame.screenshot) std::fprintf(stderr, "[gpu] screenshot saved: %s\n", Screenshot::Write(GetAppTitleId_nid_postfix().value, frame.width, frame.height, frame.pixels).string().c_str());
                    else WriteFrameBmp(frame.index, frame.width, frame.height, frame.pixels, frame.scale);
                    std::lock_guard lock(mutex);
                    queuedBytes -= frame.pixels.size();
                    changed.notify_all();
                }
            }
        } catch (const std::exception& error) {
            std::fprintf(stderr, "[gpu] frame dump writer failed: %s\n", error.what());
            std::terminate();
        }
    }

    static constexpr std::size_t maxBytes = 64 * 1024 * 1024;
    std::mutex mutex;
    std::condition_variable changed;
    std::deque<Frame> pending;
    std::thread worker;
    std::size_t queuedBytes = 0;
    bool stopping = false;
};

// The [present] line's counters (see the header); mutated under State::presentMutex.
VulkanDevice::PresentStatistics presentCounters{};

}

// A compute dispatch's Vulkan objects, shared by every dispatch of the same compiled variant and
// kept by the recorder until the batches using them completed (a Recipe references them weakly).
struct ComputePipelineObjects {
    VkDevice device = VK_NULL_HANDLE;
    PFN_vkDestroyShaderModule destroyModule = nullptr;
    PFN_vkDestroyPipelineLayout destroyLayout = nullptr;
    PFN_vkDestroyPipeline destroyPipeline = nullptr;
    VkShaderModule module = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout resourcesLayout = VK_NULL_HANDLE;
    std::mutex splitMutex;
    std::map<std::array<std::uint32_t, 3>, VkPipeline> splitPipelines;
    ~ComputePipelineObjects() {
        for (const auto& [base, split] : splitPipelines) destroyPipeline(device, split, nullptr);
        if (pipeline != VK_NULL_HANDLE) destroyPipeline(device, pipeline, nullptr);
        if (layout != VK_NULL_HANDLE) destroyLayout(device, layout, nullptr);
        if (module != VK_NULL_HANDLE) destroyModule(device, module, nullptr);
    }
};

// Pipelines are shared by dispatches of one module (the result's run-stable identity) with the
// same push constant use; descriptor set layouts built from the same bindings are compatible, so
// the pipeline layout of the first dispatch, or of the journal replay, serves them all.
struct ComputePipelineKey {
    ShaderRecompiler::PipelineIdentity identity;
    bool pushConstants = false;
    auto operator<=>(const ComputePipelineKey&) const = default;
    static ComputePipelineKey Of(const ShaderRecompiler::RecompileResult& shader, bool pushConstants) { return {shader.Identity(), pushConstants}; }
};

constexpr std::uint64_t WorkgroupMemoryGranule = 1u << 20u;

struct WorkgroupMemory {
    VkDevice device = VK_NULL_HANDLE;
    PFN_vkDestroyDescriptorPool destroyPool = nullptr;
    std::unique_ptr<Graphics::DeviceBuffer> buffer;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
    ~WorkgroupMemory() {
        buffer.reset();
        if (pool != VK_NULL_HANDLE) destroyPool(device, pool, nullptr);
    }
};

struct VulkanDevice::State {
    bool compileOnly = false;
    void* library = nullptr;
    PFN_vkGetInstanceProcAddr instanceProc = nullptr;
    PFN_vkGetDeviceProcAddr deviceProc = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkCommandPool pool = VK_NULL_HANDLE;
    void* window = nullptr;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkExtent2D extent{};
    std::vector<VkImage> images;
    VkFence acquireFence = VK_NULL_HANDLE;
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    struct RetiredSwapchain {
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        std::vector<VkSemaphore> rendered;
    };
    std::vector<VkSemaphore> rendered;
    std::vector<RetiredSwapchain> retiredSwapchains;
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    VkBuffer uploadBuffer = VK_NULL_HANDLE;
    VkDeviceMemory uploadMemory = VK_NULL_HANDLE;
    void* uploadMapping = nullptr;
    VkDeviceSize uploadSize = 0;
    VkPhysicalDeviceProperties properties{};
    std::string driverInfo;
    VkDriverId driverId = VK_DRIVER_ID_MAX_ENUM;
    std::uint32_t pixelFunctionStorageDwords = 4096u;
    bool individuallyFreeDescriptorSets = false;
    PFN_vkGetCalibratedTimestampsKHR calibratedTimestamps = nullptr;
    PFN_vkCmdSetCheckpointNV checkpointMarker = nullptr;
    PFN_vkCmdWriteBufferMarkerAMD bufferMarker = nullptr;
    VkBuffer markerBuffer = VK_NULL_HANDLE;
    VkDeviceMemory markerMemory = VK_NULL_HANDLE;
    void* markerMapping = nullptr;
    BufferMarkerLog markerLog;
    bool deviceLostHook = false;
    VkTimeDomainKHR hostTimeDomain = VK_TIME_DOMAIN_DEVICE_KHR;
    VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    std::vector<std::uint32_t> capabilities{1};
    std::vector<std::string_view> spirvExtensions;
    std::unique_ptr<const ShaderDeviceProfile> shaderProfile;
    bool tessellationShader = false;
    bool meshShader = false;
    bool fragmentShaderBarycentric = false;
    bool shaderTerminateInvocation = false;
    bool fusedExactArithmetic = false;
    bool geometryShader = false;
    bool largePoints = false;
    bool sampleRateShading = false;
    bool storageImageMultisample = false;
    bool storageImageWriteWithoutFormat = false;
    bool shaderClipDistance = false;
    bool shaderCullDistance = false;
    bool shaderClock = false;
    bool narrowSubgroupClock = false;
    bool float64DenormPreserve = false;
    bool float16DenormPreserve = false;
    bool float32FlushToZeroCandidate = false;
    bool float32FlushToZero = false;
    bool float32DenormPreserve = false;
    bool probingFloatControls = false;
    bool demoteToHelperInvocation = false;
    bool earlyFragmentSampleMaskTestBeforeSampleCounting = false;
    bool robustBufferAccess2 = false;
    bool subgroupControlBarriers = true;
    bool workgroupScopeFences = true;
    // VK_EXT_descriptor_indexing with non-uniform image array indexing (bindless image tables in
    // graphics stages, and compute workgroups wider than a wave).
    bool descriptorIndexing = false;
    bool storageBufferUpdateAfterBind = false;
    bool sampledImageUpdateAfterBind = false;
    bool descriptorBindingPartiallyBound = false;
    bool descriptorBindingVariableDescriptorCount = false;
    bool runtimeDescriptorArray = false;
    VkPhysicalDeviceDescriptorIndexingPropertiesEXT descriptorIndexingProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES_EXT};
    bool imageInt64Atomics = false;
    bool bufferInt64Atomics = false;
    bool viewportIndexLayer = false;
    bool imageFormatList = false;
    bool primitiveListRestart = false;
    bool depthClipControl = false;
    bool graphicsPipelineLibrary = false;
    bool pipelineLinkTimeOptimization = true;
    bool imageViewMinLod = false;
    bool pipelineExecutableInfo = false;
    bool maintenance8 = false;
    bool maintenance5 = false;
    bool fillModeNonSolid = false;
    std::uint32_t srgbDecodeFormats = 0;
    std::uint32_t srgbCopyFormats = 0;
    bool storageImageWithoutFormat = false;
    VkSampleCountFlags sampleLocationSampleCounts = 0;
    VkSampleCountFlags sampleLocationGridSampleCounts = 0;
    std::array<VkExtent2D, 4> sampleLocationGridSizes{};
    bool shaderStencilExport = false;
    bool depthClamp = false;
    bool depthBounds = false;
    bool depthBiasClamp = false;
    bool dualSrcBlend = false;
    bool occlusionQueryPrecise = false;
    VkDeviceSize hostImportAlignment = 0;
    bool drawInputsInPlace = true;
    bool dmaBufImport = false;
    bool sparseBinding = false;
    bool sparseResidencyBuffer = false;
    bool sparseResidencyAliased = false;
    std::uint32_t queueFamily = 0;
    std::uint32_t sparseFamily = VK_QUEUE_FAMILY_IGNORED;
    std::uint32_t sparseIndex = 0;
    VkQueue sparseQueue = VK_NULL_HANDLE;
    std::mutex sparseQueueMutex;
    VkQueue settleQueue = VK_NULL_HANDLE;
    std::uint32_t settleFamily = 0;
    VkCommandPool settlePool = VK_NULL_HANDLE;
    std::mutex settleQueueMutex;
    bool memoryBudget = false;
    bool depthRangeUnrestricted = false;
    bool samplerAnisotropy = false;
    bool textureCompressionBC = false;
    bool samplerFilterMinmax = false;
    bool nonSeamlessCubeMap = false;
    VkSampleCountFlags mergedCoverageSampleCounts = 0;
    VkSampleCountFlags truncatedCoverageSampleCounts = 0;
    bool mixedAttachmentSamples = false;
    bool fragmentShaderPixelInterlock = false;
    bool conservativeRasterization = false;
    bool provokingVertexLast = false;
    bool provokingVertexModePerPipeline = false;
    // VK_KHR_timeline_semaphore enabled: the recorder's unlocked waits are available.
    bool timelineSemaphores = false;
    bool computeWave32 = false;
    bool meshWave32 = false;
    std::uint32_t maxComputeSubgroupSize = 0;
    bool computeFullSubgroups = false;
    bool spannedWaves = false;
    // Indirect draw features enabled (see Graphics::Context).
    bool drawIndirectFirstInstance = false;
    bool multiDrawIndirect = false;
    bool drawIndirectCount = false;
    // The device's resolved entry points (Graphics::DeviceFunctions) and the Graphics::Context
    // built once after setup (see graphicsContext); the context's pool reference is dropped before
    // the pool at teardown.
    Graphics::DeviceFunctions deviceFunctions;
    bool functionsReady = false;
    Graphics::Context context;
    bool contextReady = false;
    std::unique_ptr<Graphics::TextureDetiler> detiler;
    std::unique_ptr<Graphics::Recorder> recorder;
    // Device-local buffers of a repeated 16-byte fill pattern (FillBuffer), most recently used
    // last; each is filled once by a doubling chain in device memory.
    std::vector<std::pair<std::array<std::uint32_t, 4>, std::shared_ptr<Graphics::DeviceBuffer>>> patternBuffers;
    VkDescriptorSetLayout workgroupMemoryLayout = VK_NULL_HANDLE;
    std::shared_ptr<WorkgroupMemory> workgroupMemory;
    // Compute pipeline objects by variant (and push-constant use), under their own mutex: the
    // dispatch's find-or-insert and the verify switch's lookups touch the map, recipes hold weak
    // references to its objects.
    AgcDriver::Mutex computePipelinesMutex;
    std::map<ComputePipelineKey, std::shared_ptr<ComputePipelineObjects>> computePipelines;
    std::map<ComputePipelineKey, std::shared_future<std::shared_ptr<ComputePipelineObjects>>> computePipelinesBuilding;
    // The journal replay building this device's pipelines on its own threads; drained first at teardown.
    std::shared_ptr<Graphics::PipelineJournalReplay> journalReplay;
    std::atomic<bool> journalReplayPending{false};
    std::atomic<std::uint64_t> computePipelinesCreated{0};
    std::unique_ptr<Graphics::GpuColorTransfer> colorTransfer;
    std::shared_ptr<Graphics::BufferPool> bufferPool;
    std::unique_ptr<Graphics::Buffer> emptyBuffer;
    std::unique_ptr<Graphics::PlaceholderImages> placeholderImages;
    std::unique_ptr<Graphics::TextureCache> textureCache;
    std::unique_ptr<Graphics::PipelineCache> pipelineCache;
    std::unique_ptr<Graphics::DescriptorCache> descriptorCache;
    std::unique_ptr<Graphics::VertexInputCache> vertexInputs;
    std::unique_ptr<Graphics::SamplerCache> samplerCache;
    Graphics::ResourceCache& resourceCache = Graphics::SharedResourceCache();
    // Recorded dispatches that write a copied buffer (their results reach guest memory by a CPU
    // write-back when the batch is reaped), listed until that write-back ran. An indirect dispatch
    // whose arguments one of them writes must read them on the CPU (DispatchIndirect); GPU-direct
    // writes into host imports need no entry. Under GuestMemory::GpuMutex; shared with the completion
    // actions so they never outlive it.
    std::shared_ptr<std::vector<std::shared_ptr<Graphics::ShaderResources>>> copiedWriters = std::make_shared<std::vector<std::shared_ptr<Graphics::ShaderResources>>>();
    VkPhysicalDeviceMeshShaderPropertiesEXT meshLimits{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT};
    std::unique_ptr<PresentationScaler> scaler;
    std::unique_ptr<PresentationPass> presentation;
    std::unique_ptr<PerformanceOverlay> overlay;
    VkFormat surfaceFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkColorSpaceKHR surfaceColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    bool swapchainColorSpaces = false;
    bool hdrMetadata = false;
    std::vector<VkSurfaceFormatKHR> surfaceFormats;
    std::uint64_t surfaceDisplayGeneration = 0;
    std::uint64_t metadataGeneration = 0;
    std::unique_ptr<PresentationScaler> hdrScaler;
    VkFormat hdrScalerSource = VK_FORMAT_UNDEFINED;
    // The swapchain image AcquireImage took for the next present(), consumed by that present.
    bool imageAcquired = false;
    std::uint32_t acquiredIndex = 0;
    // Presentations in flight (see the header): FlipInFlight() + 1 slots used round robin from
    // `presentCursor` (the slot there is the oldest). A slot's kept resident image and its dump
    // buffer (APS5_DUMP_FRAMES: the frame read back by the blit's submission) live until its fence
    // signaled. `presentMutex` orders the presenter's unlocked RetirePresents against a WaitIdle
    // from a queue worker; it is never taken before GuestMemory::GpuMutex on a thread.
    struct PresentSlot {
        VkFence fence = VK_NULL_HANDLE;
        VkCommandBuffer commands = VK_NULL_HANDLE;
        std::shared_ptr<void> kept;
        std::unique_ptr<Graphics::Buffer> dumpBuffer;
        std::unique_ptr<PresentationScaler> dumpScaler;
        std::uint32_t dumpWidth = 0;
        std::uint32_t dumpHeight = 0;
        std::uint32_t dumpScale = 1;
        bool dumpRecorded = false;
        bool dumpScreenshot = false;
        int dumpIndex = 0;
        std::uint32_t imageIndex = 0;
        bool inFlight = false;
        // The blit reads the single scaler source, staging or upload objects (not a direct blit
        // from the resident image): the next use of those waits for this slot.
        bool shared = false;
        // Recorder::Submissions() when the blit was submitted and the previous slot's: the batches
        // in between ran ahead of this blit.
        std::uint64_t recorderSerial = 0;
        std::uint64_t previousSerial = 0;
        std::chrono::steady_clock::time_point submittedAt{};
        // APS5_PROFILE_GPU: the blit's own two timestamps (the [gputime] present-blit class), read
        // when the slot retires, and the swapchain bytes it wrote.
        VkQueryPool queries = VK_NULL_HANDLE;
        std::uint64_t blitBytes = 0;
    };
    std::vector<PresentSlot> presentSlots;
    std::size_t presentCursor = 0;
    std::uint64_t lastPresentSerial = 0;
    // The batch ranges of retired slots whose GPU accounting waits for the batches' completion
    // records (SettleRetired); under presentMutex.
    struct RetiredRange {
        std::uint64_t afterSerial;
        std::uint64_t throughSerial;
    };
    std::deque<RetiredRange> retiredRanges;
    AgcDriver::Mutex presentMutex;
    // The presentation present() submitted and QueuePresent has not handed to the swapchain.
    bool queuePending = false;
    std::uint32_t queueIndex = 0;
    int nextDumpIndex = 0;
    bool nextDumpScreenshot = false;
    FrameDumpWriter dumpWriter;

    template<typename TFunction>
    TFunction InstanceFunction(const char* name) const {
        auto function = reinterpret_cast<TFunction>(instanceProc(instance, name));
        if (function == nullptr) {
            throw std::runtime_error(std::string("Vulkan instance function missing: ") + name);
        }
        return function;
    }

    template<typename TFunction>
    TFunction DeviceFunction(const char* name) const {
        ++Graphics::DeviceProcLookups();
        auto function = reinterpret_cast<TFunction>(deviceProc(device, name));
        if (function == nullptr) {
            throw std::runtime_error(std::string("Vulkan device function missing: ") + name);
        }
        return function;
    }

    void CreateWorkgroupMemoryLayout() {
        const VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
        VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        info.bindingCount = 1;
        info.pBindings = &binding;
        check(DeviceFunction<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(device, &info, nullptr, &workgroupMemoryLayout), "vkCreateDescriptorSetLayout workgroup memory");
    }

    std::shared_ptr<WorkgroupMemory> WorkgroupMemoryFor(const Graphics::Context& context, std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint32_t dwords) {
        const std::uint64_t range = properties.limits.maxStorageBufferRange;
        const auto groupBytes = static_cast<std::uint64_t>(dwords) * 4u;
        std::uint64_t groups = 1;
        for (const std::uint64_t count : {x, y, z}) {
            if (count != 0u && groups > range / groupBytes / count) {
                throw std::runtime_error("Vulkan dispatch: the LDS of " + std::to_string(x) + "x" + std::to_string(y) + "x" + std::to_string(z) + " workgroups of " + std::to_string(groupBytes) + " bytes each is over maxStorageBufferRange " + std::to_string(range));
            }
            groups *= count;
        }
        const auto bytes = std::max<std::uint64_t>(groups, 1u) * groupBytes;
        if (workgroupMemory != nullptr && workgroupMemory->buffer->Size() >= bytes) return workgroupMemory;
        const auto size = std::min<std::uint64_t>((bytes + WorkgroupMemoryGranule - 1u) / WorkgroupMemoryGranule * WorkgroupMemoryGranule, range);
        auto memory = std::make_shared<WorkgroupMemory>();
        memory->device = device;
        memory->destroyPool = DeviceFunction<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool");
        memory->buffer = std::make_unique<Graphics::DeviceBuffer>(context, static_cast<std::size_t>(size), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        const VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        check(DeviceFunction<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(device, &poolInfo, nullptr, &memory->pool), "vkCreateDescriptorPool workgroup memory");
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocation.descriptorPool = memory->pool;
        allocation.descriptorSetCount = 1;
        allocation.pSetLayouts = &workgroupMemoryLayout;
        check(DeviceFunction<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(device, &allocation, &memory->set), "vkAllocateDescriptorSets workgroup memory");
        const VkDescriptorBufferInfo described{memory->buffer->Handle(), 0, size};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = memory->set;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.pBufferInfo = &described;
        DeviceFunction<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(device, 1, &write, 0, nullptr);
        workgroupMemory = std::move(memory);
        return workgroupMemory;
    }

    void CreateMarkerBuffer() {
        bufferMarker = DeviceFunction<PFN_vkCmdWriteBufferMarkerAMD>("vkCmdWriteBufferMarkerAMD");
        VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        buffer.size = 2 * sizeof(std::uint32_t);
        buffer.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        check(DeviceFunction<PFN_vkCreateBuffer>("vkCreateBuffer")(device, &buffer, nullptr, &markerBuffer), "vkCreateBuffer buffer markers");
        VkMemoryRequirements requirements{};
        DeviceFunction<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(device, markerBuffer, &requirements);
        std::uint32_t memoryType = memoryProperties.memoryTypeCount;
        const auto flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        for (std::uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
            if ((requirements.memoryTypeBits & (1u << i)) != 0 && (memoryProperties.memoryTypes[i].propertyFlags & flags) == flags) {
                memoryType = i;
                break;
            }
        }
        require(memoryType < memoryProperties.memoryTypeCount, "coherent host memory for buffer markers is unavailable");
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = memoryType;
        check(DeviceFunction<PFN_vkAllocateMemory>("vkAllocateMemory")(device, &allocation, nullptr, &markerMemory), "vkAllocateMemory buffer markers");
        check(DeviceFunction<PFN_vkBindBufferMemory>("vkBindBufferMemory")(device, markerBuffer, markerMemory, 0), "vkBindBufferMemory buffer markers");
        check(DeviceFunction<PFN_vkMapMemory>("vkMapMemory")(device, markerMemory, 0, buffer.size, 0, &markerMapping), "vkMapMemory buffer markers");
        std::memset(markerMapping, 0, buffer.size);
    }

    void Upload(std::span<const std::byte> pixels) {
        if (uploadSize < pixels.size()) {
            if (uploadMapping) DeviceFunction<PFN_vkUnmapMemory>("vkUnmapMemory")(device, uploadMemory);
            uploadMapping = nullptr;
            if (uploadBuffer) DeviceFunction<PFN_vkDestroyBuffer>("vkDestroyBuffer")(device, uploadBuffer, nullptr);
            uploadBuffer = VK_NULL_HANDLE;
            if (uploadMemory) DeviceFunction<PFN_vkFreeMemory>("vkFreeMemory")(device, uploadMemory, nullptr);
            uploadMemory = VK_NULL_HANDLE;
            uploadSize = 0;
            VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            buffer.size = pixels.size();
            buffer.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            check(DeviceFunction<PFN_vkCreateBuffer>("vkCreateBuffer")(device, &buffer, nullptr, &uploadBuffer), "vkCreateBuffer display upload");
            VkMemoryRequirements requirements{};
            DeviceFunction<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(device, uploadBuffer, &requirements);
            std::uint32_t memoryType = memoryProperties.memoryTypeCount;
            const auto flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            for (std::uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
                if ((requirements.memoryTypeBits & (1u << i)) != 0 && (memoryProperties.memoryTypes[i].propertyFlags & flags) == flags) {
                    memoryType = i;
                    break;
                }
            }
            require(memoryType < memoryProperties.memoryTypeCount, "coherent host upload memory is unavailable");
            VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocation.allocationSize = requirements.size;
            allocation.memoryTypeIndex = memoryType;
            check(DeviceFunction<PFN_vkAllocateMemory>("vkAllocateMemory")(device, &allocation, nullptr, &uploadMemory), "vkAllocateMemory display upload");
            check(DeviceFunction<PFN_vkBindBufferMemory>("vkBindBufferMemory")(device, uploadBuffer, uploadMemory, 0), "vkBindBufferMemory display upload");
            check(DeviceFunction<PFN_vkMapMemory>("vkMapMemory")(device, uploadMemory, 0, pixels.size(), 0, &uploadMapping), "vkMapMemory display upload");
            uploadSize = pixels.size();
        }
        std::memcpy(uploadMapping, pixels.data(), pixels.size());
    }

    bool SharedSlotInFlight() {
        std::lock_guard lock(presentMutex);
        return std::any_of(presentSlots.begin(), presentSlots.end(), [](const auto& slot) { return slot.inFlight && slot.shared; });
    }

    // Whether a recorded dispatch's or draw's copied write-back (listed until it ran) stores into
    // the range on the CPU; under GpuMutex.
    bool CopiedWriterOverlaps(std::uint64_t address, std::size_t bytes) const {
        const auto overlaps = [&](const auto& writer) { return writer->CopiedWritesOverlap(address, bytes); };
        return std::any_of(copiedWriters->begin(), copiedWriters->end(), overlaps) || std::any_of(Graphics::DrawCopiedWriters()->begin(), Graphics::DrawCopiedWriters()->end(), overlaps);
    }

    void DestroyRetiredSwapchains() {
        if (retiredSwapchains.empty()) return;
        const auto destroySemaphore = DeviceFunction<PFN_vkDestroySemaphore>("vkDestroySemaphore");
        const auto destroySwapchain = DeviceFunction<PFN_vkDestroySwapchainKHR>("vkDestroySwapchainKHR");
        for (const auto& retired : retiredSwapchains) {
            for (auto semaphore : retired.rendered) {
                if (semaphore) destroySemaphore(device, semaphore, nullptr);
            }
            destroySwapchain(device, retired.swapchain, nullptr);
        }
        retiredSwapchains.clear();
    }

    // A presentation slot whose fence signaled: its frame dump is written, its kept image released
    // and its statistics taken (under presentMutex).
    void RetireSlot(PresentSlot& slot) {
        slot.inFlight = false;
        if (presentation) presentation->Release(static_cast<std::size_t>(&slot - presentSlots.data()));
        slot.kept.reset();
        if (slot.dumpRecorded) {
            slot.dumpRecorded = false;
            WriteFrameDump(slot);
        }
        if (slot.queries != VK_NULL_HANDLE) {
            std::uint64_t stamps[2] = {};
            if (DeviceFunction<PFN_vkGetQueryPoolResults>("vkGetQueryPoolResults")(device, slot.queries, 0, 2, sizeof(stamps), stamps, sizeof(std::uint64_t), VK_QUERY_RESULT_64_BIT) == VK_SUCCESS && stamps[1] > stamps[0]) {
                Graphics::Recorder::AddGpuTiming(Graphics::Recorder::CommandClass::PresentBlit, static_cast<double>(stamps[1] - stamps[0]) * properties.limits.timestampPeriod, slot.blitBytes);
            }
        }
        if (!recorder) return;
        const auto afterFlip = recorder->NewestSubmitted() - slot.recorderSerial;
        presentCounters.batchesAfterFlip += afterFlip;
        if (auto* frame = PerformanceContext::Current()) frame->NoteRetired(afterFlip, 0, 0);
        retiredRanges.push_back({slot.previousSerial, slot.recorderSerial});
        SettleRetired(false);
    }

    // The GPU accounting of retired slots ([present] line, frame record): a batch's completion
    // record is written when a queue worker finishes it, which can be well after the blit's fence
    // signaled (compute queues never reap), so a range read at the retire was mostly unread. Each
    // range waits until every serial in it has a record, or until the ring could have dropped its
    // oldest one (`force`: at teardown). Under presentMutex.
    void SettleRetired(bool force) {
        const auto newest = recorder->NewestSubmitted();
        while (!retiredRanges.empty()) {
            const auto range = retiredRanges.front();
            std::size_t missing = 0;
            const auto batches = recorder->CompletedBatches(range.afterSerial, range.throughSerial, missing);
            if (missing != 0 && !force && newest < range.afterSerial + Graphics::Recorder::CompletedRingSize / 2) break;
            retiredRanges.pop_front();
            double busyMs = 0, firstNs = 0, lastNs = 0;
            bool stamped = false;
            std::uint64_t checked = 0, overwritten = 0;
            for (const auto& batch : batches) {
                if (batch.gpuEndNs > batch.gpuStartNs) {
                    busyMs += (batch.gpuEndNs - batch.gpuStartNs) / 1e6;
                    firstNs = stamped ? std::min(firstNs, batch.gpuStartNs) : batch.gpuStartNs;
                    lastNs = stamped ? std::max(lastNs, batch.gpuEndNs) : batch.gpuEndNs;
                    stamped = true;
                }
                if (batch.readGeneration == 0) continue;
                for (const auto& [begin, end] : batch.reads) {
                    ++checked;
                    GuestMemory::CollectWritesUncached(begin, end - begin);
                    if (!GuestMemory::UnchangedSince(begin, end - begin, batch.readGeneration)) ++overwritten;
                }
            }
            const double gapMs = stamped ? (lastNs - firstNs) / 1e6 - busyMs : 0;
            if (auto* frame = PerformanceContext::Current()) frame->NoteRetired(0, busyMs, gapMs);
            presentCounters.gpuBusyMs += busyMs;
            presentCounters.gpuGapMs += gapMs;
            presentCounters.gpuUnread += missing;
            presentCounters.readsChecked += checked;
            presentCounters.readsOverwritten += overwritten;
        }
    }

    void WriteFrameDump(const PresentSlot& slot) {
        require(slot.dumpBuffer != nullptr, "frame dump was not recorded");
        const auto full = slot.dumpBuffer->Bytes();
        const auto index = slot.dumpIndex;
        if (slot.dumpScreenshot) {
            require(full.size() == static_cast<std::size_t>(slot.dumpWidth) * slot.dumpHeight * 4, "invalid screenshot pixels");
            dumpWriter.Enqueue(index, slot.dumpWidth, slot.dumpHeight, 1, true, std::vector<std::byte>(full.begin(), full.end()));
            return;
        }
        const auto scale = slot.dumpScale;
        require(scale != 0 && full.size() == static_cast<std::size_t>(slot.dumpWidth) * slot.dumpHeight * 4, "invalid frame dump pixels");
        const auto width = (slot.dumpWidth + scale - 1) / scale;
        const auto height = (slot.dumpHeight + scale - 1) / scale;
        std::vector<std::byte> pixels(static_cast<std::size_t>(width) * height * 4);
        for (std::uint32_t y = 0; y < height; ++y) {
            for (std::uint32_t x = 0; x < width; ++x) std::memcpy(pixels.data() + (static_cast<std::size_t>(y) * width + x) * 4, full.data() + (static_cast<std::size_t>(y) * scale * slot.dumpWidth + static_cast<std::size_t>(x) * scale) * 4, 4);
        }
        static const auto start = std::chrono::steady_clock::now();
        std::fprintf(stderr, "[gpu] frame_%03d.bmp: %ux%u display read back on the GPU at %.1f s\n", index, slot.dumpWidth, slot.dumpHeight, std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
        dumpWriter.Enqueue(index, width, height, 1, false, std::move(pixels));
    }

    // Blocks on the fence, or polls it every 50 us with APS5_PRESENT_POLL_FENCE=1 (the difference
    // in the wait is the presenter's wake-up latency).
    void WaitPresentFence(VkFence fence) {
        static const bool poll = std::getenv("APS5_PRESENT_POLL_FENCE") != nullptr;
        if (!poll) {
            check(DeviceFunction<PFN_vkWaitForFences>("vkWaitForFences")(device, 1, &fence, VK_TRUE, std::numeric_limits<std::uint64_t>::max()), "vkWaitForFences present");
            return;
        }
        const auto status = DeviceFunction<PFN_vkGetFenceStatus>("vkGetFenceStatus");
        for (;;) {
            const auto result = status(device, fence);
            if (result == VK_SUCCESS) return;
            if (result != VK_NOT_READY) check(result, "vkGetFenceStatus present");
            const auto until = std::chrono::steady_clock::now() + std::chrono::microseconds(50);
            while (std::chrono::steady_clock::now() < until) std::this_thread::yield();
        }
    }

    ~State() {
        if (journalReplay != nullptr) {
            journalReplay->Cancel();
            journalReplay->Wait();
            journalReplay.reset();
            Graphics::ReportPipelineJournal();
        }
        contextReady = false;
        if (deviceLostHook && Graphics::DeviceLostOwner() == this) {
            Graphics::DeviceLostHook() = nullptr;
            Graphics::DeviceLostOwner() = nullptr;
        }
        if (compileOnly && device != VK_NULL_HANDLE) {
            reinterpret_cast<PFN_vkDestroyDevice>(deviceProc(device, "vkDestroyDevice"))(device, nullptr);
            device = VK_NULL_HANDLE;
        }
        if (device != VK_NULL_HANDLE) {
            const auto idle = reinterpret_cast<PFN_vkDeviceWaitIdle>(deviceProc(device, "vkDeviceWaitIdle"))(device);
            if (idle != VK_SUCCESS && idle != VK_ERROR_DEVICE_LOST) std::terminate();
            // Resident images kept for unfinished presentations go before the caches they came from.
            {
                std::lock_guard lock(presentMutex);
                for (auto& slot : presentSlots) {
                    if (slot.inFlight) RetireSlot(slot);
                }
                if (recorder) SettleRetired(true);
            }
            for (auto& slot : presentSlots) {
                slot.dumpBuffer.reset();
                slot.dumpScaler.reset();
            }
            // Results still in the unit shadows reach guest memory before the device goes; the
            // slabs (kept by the publishing batch) are freed by the recorder's teardown.
            if (recorder) {
                try {
                    Graphics::PublishAllShadows(context, Graphics::PublishReason::Teardown);
                    recorder->Sync();
                } catch (const std::exception& error) {
                    std::fprintf(stderr, "[gpu] unit shadow teardown: %s\n", error.what());
                }
            }
            Graphics::DestroyShadows(device);
            recorder.reset();
            // Cached graphics pipelines (with their framebuffers, modules, render passes and layouts)
            // belong to this device and must be destroyed while it lives.
            Graphics::ClearCachedPipelines(device);
            Graphics::ClearDepthSurfaces(device);
            Graphics::ClearMultisampleTargets(device);
            Graphics::ClearSampleOrders(device);
            {
                std::lock_guard pipelines(computePipelinesMutex);
                computePipelines.clear();
                computePipelinesBuilding.clear();
            }
            // Every ShaderResources (kept by the recorder or the resource cache) is gone now, so the
            // sets and samplers they borrowed can go.
            copiedWriters->clear();
            resourceCache.Clear();
            Graphics::ClearCachedTextures(device);
            Graphics::ClearSnapshotCopies(device);
            Graphics::ClearImageMirrors(device);
            Graphics::ClearHostImports(device);
            patternBuffers.clear();
            workgroupMemory.reset();
            if (workgroupMemoryLayout != VK_NULL_HANDLE) reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(deviceProc(device, "vkDestroyDescriptorSetLayout"))(device, workgroupMemoryLayout, nullptr);
            descriptorCache.reset();
            vertexInputs.reset();
            emptyBuffer.reset();
            placeholderImages.reset();
            samplerCache.reset();
            textureCache.reset();
            detiler.reset();
            colorTransfer.reset();
            overlay.reset();
            presentation.reset();
            scaler.reset();
            hdrScaler.reset();
            pipelineCache.reset();
            context.bufferPool.reset();
            bufferPool.reset();
            const auto destroyFence = reinterpret_cast<PFN_vkDestroyFence>(deviceProc(device, "vkDestroyFence"));
            if (acquireFence) destroyFence(device, acquireFence, nullptr);
            const auto destroyQueryPool = reinterpret_cast<PFN_vkDestroyQueryPool>(deviceProc(device, "vkDestroyQueryPool"));
            for (const auto& slot : presentSlots) {
                if (slot.fence) destroyFence(device, slot.fence, nullptr);
                if (slot.queries) destroyQueryPool(device, slot.queries, nullptr);
            }
            const auto destroySemaphore = reinterpret_cast<PFN_vkDestroySemaphore>(deviceProc(device, "vkDestroySemaphore"));
            for (auto semaphore : rendered) {
                if (semaphore) destroySemaphore(device, semaphore, nullptr);
            }
            DestroyRetiredSwapchains();
            if (uploadMapping) reinterpret_cast<PFN_vkUnmapMemory>(deviceProc(device, "vkUnmapMemory"))(device, uploadMemory);
            if (uploadBuffer) reinterpret_cast<PFN_vkDestroyBuffer>(deviceProc(device, "vkDestroyBuffer"))(device, uploadBuffer, nullptr);
            if (uploadMemory) reinterpret_cast<PFN_vkFreeMemory>(deviceProc(device, "vkFreeMemory"))(device, uploadMemory, nullptr);
            if (markerMapping) reinterpret_cast<PFN_vkUnmapMemory>(deviceProc(device, "vkUnmapMemory"))(device, markerMemory);
            if (markerBuffer) reinterpret_cast<PFN_vkDestroyBuffer>(deviceProc(device, "vkDestroyBuffer"))(device, markerBuffer, nullptr);
            if (markerMemory) reinterpret_cast<PFN_vkFreeMemory>(deviceProc(device, "vkFreeMemory"))(device, markerMemory, nullptr);
            if (swapchain) reinterpret_cast<PFN_vkDestroySwapchainKHR>(deviceProc(device, "vkDestroySwapchainKHR"))(device, swapchain, nullptr);
            const auto destroyPool = reinterpret_cast<PFN_vkDestroyCommandPool>(deviceProc(device, "vkDestroyCommandPool"));
            const auto destroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(deviceProc(device, "vkDestroyDevice"));
            if (pool != VK_NULL_HANDLE) {
                destroyPool(device, pool, nullptr);
            }
            if (settlePool != VK_NULL_HANDLE) destroyPool(device, settlePool, nullptr);
            destroyDevice(device, nullptr);
        }
        if (instance != VK_NULL_HANDLE) {
            if (surface) reinterpret_cast<PFN_vkDestroySurfaceKHR>(instanceProc(instance, "vkDestroySurfaceKHR"))(instance, surface, nullptr);
            reinterpret_cast<PFN_vkDestroyInstance>(instanceProc(instance, "vkDestroyInstance"))(instance, nullptr);
        }
        if (library != nullptr) {
            SDL_UnloadObject(library);
        }
    }
};

ComputePipelineReplayResult VulkanDevice::ReplayComputePipeline(const ComputePipelineCapture& capture, std::span<const std::byte> initialCache) {
    require(state->compileOnly, "pipeline replay requires a compile-only device");
    const auto start = std::chrono::steady_clock::now();
    const auto elapsed = [](auto since) { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - since).count(); };
    capture.Validate();
    require(initialCache.size() <= 64u * 1024u * 1024u, "replay pipeline cache exceeds 64 MiB");
    if (!initialCache.empty()) {
        require(initialCache.size() >= 32, "replay pipeline cache header is truncated");
        const auto word = [&](std::size_t offset) {
            std::uint32_t result = 0;
            for (unsigned shift = 0; shift < 32; shift += 8) result |= std::to_integer<std::uint32_t>(initialCache[offset++]) << shift;
            return result;
        };
        require(word(0) >= 32 && word(0) <= initialCache.size() && word(4) == VK_PIPELINE_CACHE_HEADER_VERSION_ONE, "unsupported replay pipeline cache header");
        require(word(8) == state->properties.vendorID && word(12) == state->properties.deviceID, "replay pipeline cache belongs to another device");
        for (std::size_t index = 0; index < VK_UUID_SIZE; ++index) require(std::to_integer<std::uint8_t>(initialCache[16 + index]) == state->properties.pipelineCacheUUID[index], "replay pipeline cache UUID differs from the driver");
    }
    require((capture.flags & VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR) == 0 || state->pipelineExecutableInfo, "captured pipeline statistics feature is not enabled");
    if (capture.requiredSubgroupSize != 0) {
        VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT};
        VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &subgroup};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(state->physical, &properties);
        require(capture.stageFlags == 0 || state->computeFullSubgroups, "captured full compute subgroups are unsupported");
        require(state->computeWave32 && capture.requiredSubgroupSize >= subgroup.minSubgroupSize && capture.requiredSubgroupSize <= subgroup.maxSubgroupSize && (subgroup.requiredSubgroupSizeStages & VK_SHADER_STAGE_COMPUTE_BIT) != 0, "captured required subgroup size is unsupported");
    }
    const auto target = Target();
    require(capture.spirv[1] <= target.spirvVersion, "captured SPIR-V version exceeds enabled device support");
    for (std::size_t cursor = 5; cursor < capture.spirv.size(); cursor += capture.spirv[cursor] >> 16u) {
        const auto count = capture.spirv[cursor] >> 16u;
        if ((capture.spirv[cursor] & 0xffffu) == spv::OpCapability) {
            require(count == 2 && (CoreVulkanCapability(capture.spirv[cursor + 1]) || std::ranges::find(target.supportedCapabilities, capture.spirv[cursor + 1]) != target.supportedCapabilities.end()), "captured SPIR-V capability is not enabled");
        }
    }
#if ANYPS5_ENABLE_SPIRV_TOOLS
    static_cast<void>(ShaderRecompiler::ValidateAndOptimizeSpirv(capture.spirv, target.vulkanVersion, target.spirvVersion, target.nonConstantImageOffsets, false));
#else
    throw std::runtime_error("compute pipeline replay requires ANYPS5_ENABLE_SPIRV_TOOLS=ON");
#endif
    ShaderRecompiler::RecompileResult shader;
    shader.spirv = capture.spirv;
    for (const auto& value : capture.specialization) shader.specialization.push_back({value.id, value.value});
    const Graphics::PipelineSpecialization specialization(shader);
    const auto& limits = state->properties.limits;
    require(capture.sets.size() <= limits.maxBoundDescriptorSets, "captured descriptor sets exceed device limits");
    for (const auto& range : capture.pushConstants) require(range.offset + range.size <= limits.maxPushConstantsSize, "captured push constants exceed device limits");
    struct ReplayObjects {
        VkDevice device;
        PFN_vkDestroyDescriptorSetLayout destroySetLayout;
        PFN_vkDestroyPipelineCache destroyCache;
        std::vector<VkDescriptorSetLayout> sets;
        VkPipelineCache cache = VK_NULL_HANDLE;
        ~ReplayObjects() {
            for (auto layout : sets) destroySetLayout(device, layout, nullptr);
            if (cache != VK_NULL_HANDLE) destroyCache(device, cache, nullptr);
        }
    } replay{state->device, state->DeviceFunction<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout"), state->DeviceFunction<PFN_vkDestroyPipelineCache>("vkDestroyPipelineCache"), {}};
    ComputePipelineObjects objects;
    objects.device = state->device;
    objects.destroyModule = state->DeviceFunction<PFN_vkDestroyShaderModule>("vkDestroyShaderModule");
    objects.destroyLayout = state->DeviceFunction<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout");
    objects.destroyPipeline = state->DeviceFunction<PFN_vkDestroyPipeline>("vkDestroyPipeline");
    ComputePipelineReplayResult result;
    result.deviceName = state->properties.deviceName;
    result.vendorId = state->properties.vendorID;
    result.deviceId = state->properties.deviceID;
    result.driverVersion = state->properties.driverVersion;
    result.apiVersion = state->properties.apiVersion;
    std::copy_n(state->properties.pipelineCacheUUID, VK_UUID_SIZE, result.pipelineCacheUuid.begin());
    result.initialCacheBytes = initialCache.size();
    auto phase = std::chrono::steady_clock::now();
    VkPipelineCacheCreateInfo cacheInfo{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    cacheInfo.initialDataSize = initialCache.size();
    cacheInfo.pInitialData = initialCache.data();
    check(state->DeviceFunction<PFN_vkCreatePipelineCache>("vkCreatePipelineCache")(state->device, &cacheInfo, nullptr, &replay.cache), "vkCreatePipelineCache replay");
    result.cacheMilliseconds = elapsed(phase);
    phase = std::chrono::steady_clock::now();
    objects.module = Graphics::CreateShaderModule(state->DeviceFunction<PFN_vkCreateShaderModule>("vkCreateShaderModule"), state->device, state->graphicsPipelineLibrary, capture.spirv.data(), capture.spirv.size() * sizeof(std::uint32_t), "vkCreateShaderModule replay");
    result.moduleMilliseconds = elapsed(phase);
    phase = std::chrono::steady_clock::now();
    std::array<std::array<std::uint64_t, 4>, 2> descriptorCounts{};
    bool hasUpdateAfterBindLayout = false;
    for (const auto& layout : capture.sets) {
        const bool updateAfterBind = (layout.flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT) != 0;
        hasUpdateAfterBindLayout |= updateAfterBind;
        require(!updateAfterBind || state->storageBufferUpdateAfterBind || state->sampledImageUpdateAfterBind, "captured update-after-bind descriptors are unsupported");
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        std::vector<VkDescriptorBindingFlags> flags;
        std::uint32_t variableCount = 0;
        for (const auto& binding : layout.bindings) {
            require((binding.flags & VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT) == 0 || state->descriptorBindingPartiallyBound, "captured partially-bound descriptors are unsupported");
            require((binding.flags & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT) == 0 || state->descriptorBindingVariableDescriptorCount, "captured variable descriptor count is unsupported");
            require((binding.flags & VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT) == 0 || (binding.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ? state->storageBufferUpdateAfterBind : state->sampledImageUpdateAfterBind), "captured update-after-bind descriptors are unsupported");
            if ((binding.flags & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT) != 0) variableCount = binding.count;
            bindings.push_back({binding.binding, binding.type, binding.count, binding.stages, nullptr});
            flags.push_back(binding.flags);
            const std::size_t kind = binding.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ? 0 : binding.type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ? 1 : binding.type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ? 2 : 3;
            descriptorCounts[updateAfterBind ? 1 : 0][kind] += binding.count;
        }
        VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
        bindingFlags.bindingCount = static_cast<std::uint32_t>(flags.size());
        bindingFlags.pBindingFlags = flags.data();
        VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        info.pNext = std::ranges::any_of(flags, [](auto value) { return value != 0; }) ? &bindingFlags : nullptr;
        info.flags = layout.flags;
        info.bindingCount = static_cast<std::uint32_t>(bindings.size());
        info.pBindings = bindings.data();
        VkDescriptorSetVariableDescriptorCountLayoutSupport variableSupport{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT};
        VkDescriptorSetLayoutSupport support{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT, variableCount != 0 ? &variableSupport : nullptr};
        state->DeviceFunction<PFN_vkGetDescriptorSetLayoutSupport>("vkGetDescriptorSetLayoutSupport")(state->device, &info, &support);
        require(support.supported == VK_TRUE, "captured descriptor set layout is unsupported");
        require(variableCount <= variableSupport.maxVariableDescriptorCount, "captured variable descriptor count exceeds layout support");
        VkDescriptorSetLayout handle = VK_NULL_HANDLE;
        check(state->DeviceFunction<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(state->device, &info, nullptr, &handle), "vkCreateDescriptorSetLayout replay");
        replay.sets.push_back(handle);
    }
    const std::array<std::uint32_t, 4> regularLimits{std::min(limits.maxDescriptorSetStorageBuffers, limits.maxPerStageDescriptorStorageBuffers), std::min(limits.maxDescriptorSetSampledImages, limits.maxPerStageDescriptorSampledImages), std::min(limits.maxDescriptorSetStorageImages, limits.maxPerStageDescriptorStorageImages), std::min(limits.maxDescriptorSetSamplers, limits.maxPerStageDescriptorSamplers)};
    const auto& indexed = state->descriptorIndexingProperties;
    const std::array<std::uint32_t, 4> indexedLimits{std::min(indexed.maxDescriptorSetUpdateAfterBindStorageBuffers, indexed.maxPerStageDescriptorUpdateAfterBindStorageBuffers), std::min(indexed.maxDescriptorSetUpdateAfterBindSampledImages, indexed.maxPerStageDescriptorUpdateAfterBindSampledImages), std::min(indexed.maxDescriptorSetUpdateAfterBindStorageImages, indexed.maxPerStageDescriptorUpdateAfterBindStorageImages), std::min(indexed.maxDescriptorSetUpdateAfterBindSamplers, indexed.maxPerStageDescriptorUpdateAfterBindSamplers)};
    std::uint64_t regularResources = 0;
    std::uint64_t indexedResources = 0;
    for (std::size_t kind = 0; kind < regularLimits.size(); ++kind) {
        require(descriptorCounts[0][kind] <= regularLimits[kind], "captured pipeline descriptors exceed device limits");
        require(!hasUpdateAfterBindLayout || descriptorCounts[0][kind] + descriptorCounts[1][kind] <= indexedLimits[kind], "captured update-after-bind pipeline descriptors exceed device limits");
        if (kind != 3) {
            regularResources += descriptorCounts[0][kind];
            indexedResources += descriptorCounts[0][kind] + descriptorCounts[1][kind];
        }
    }
    require(regularResources <= limits.maxPerStageResources && (!hasUpdateAfterBindLayout || indexedResources <= indexed.maxPerStageUpdateAfterBindResources), "captured pipeline resources exceed device limits");
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = static_cast<std::uint32_t>(replay.sets.size());
    layoutInfo.pSetLayouts = replay.sets.data();
    layoutInfo.pushConstantRangeCount = static_cast<std::uint32_t>(capture.pushConstants.size());
    layoutInfo.pPushConstantRanges = capture.pushConstants.data();
    check(state->DeviceFunction<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(state->device, &layoutInfo, nullptr, &objects.layout), "vkCreatePipelineLayout replay");
    result.layoutMilliseconds = elapsed(phase);
    phase = std::chrono::steady_clock::now();
    CreateComputePipeline(state->device, state->DeviceFunction<PFN_vkCreateComputePipelines>("vkCreateComputePipelines"), replay.cache, objects.module, objects.layout, capture.flags, capture.stageFlags, specialization.Info(), capture.requiredSubgroupSize, &objects.pipeline);
    result.pipelineMilliseconds = elapsed(phase);
    phase = std::chrono::steady_clock::now();
    const auto getCache = state->DeviceFunction<PFN_vkGetPipelineCacheData>("vkGetPipelineCacheData");
    std::size_t cacheBytes = 0;
    check(getCache(state->device, replay.cache, &cacheBytes, nullptr), "vkGetPipelineCacheData size replay");
    require(cacheBytes <= 64u * 1024u * 1024u, "exported replay pipeline cache exceeds 64 MiB");
    result.cache.resize(cacheBytes);
    check(getCache(state->device, replay.cache, &cacheBytes, result.cache.data()), "vkGetPipelineCacheData replay");
    result.cache.resize(cacheBytes);
    result.cacheMilliseconds += elapsed(phase);
    result.totalMilliseconds = elapsed(start);
    return result;
}

VulkanDevice::VulkanDevice(const PresentationWindow* window) : VulkanDevice(window, false) {}

VulkanDevice::VulkanDevice(CompileOnly) : VulkanDevice(nullptr, true) {}

VulkanDevice::VulkanDevice(const PresentationWindow* window, bool compileOnly) : state(std::make_unique<State>()), serial([] { static std::atomic<std::uint64_t> serials{0}; return serials.fetch_add(1, std::memory_order_relaxed) + 1; }()) {
    state->compileOnly = compileOnly;
    APS5_LOG_OUT("VulkanDevice constructor window=%p", static_cast<const void*>(window));
#ifdef _WIN32
    state->library = SDL_LoadObject("vulkan-1.dll");
#else
    state->library = SDL_LoadObject("libvulkan.so.1");
#endif
    if (state->library == nullptr) {
        throw std::runtime_error(std::string("Vulkan loader: ") + SDL_GetError());
    }
    APS5_LOG_OUT("Vulkan loader loaded library=%p", state->library);
    state->instanceProc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_LoadFunction(state->library, "vkGetInstanceProcAddr"));
    if (state->instanceProc == nullptr) {
        throw std::runtime_error("Vulkan loader: vkGetInstanceProcAddr missing");
    }
    AgcDriverLockVulkanLoader_nid_postfix();
    struct LoaderUnlock {
        ~LoaderUnlock() { AgcDriverUnlockVulkanLoader_nid_postfix(); }
    } loaderUnlock;
    VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    application.pApplicationName = "AnyPS5 libSceAgcDriver";
    application.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create.pApplicationInfo = &application;
    std::vector<const char*> instanceExtensions;
    if (window != nullptr) {
        APS5_LOG_OUT("Presentation window context=%p extent=%ux%u extensions=%zu", window->context, window->width, window->height, window->extensions.size());
        require(window->context && window->createSurface && window->getDrawableSize && window->width && window->height, "invalid window descriptor");
        instanceExtensions.assign(window->extensions.begin(), window->extensions.end());
        std::uint32_t availableCount = 0;
        const auto enumerateExtensions = state->InstanceFunction<PFN_vkEnumerateInstanceExtensionProperties>("vkEnumerateInstanceExtensionProperties");
        check(enumerateExtensions(nullptr, &availableCount, nullptr), "vkEnumerateInstanceExtensionProperties");
        std::vector<VkExtensionProperties> available(availableCount);
        check(enumerateExtensions(nullptr, &availableCount, available.data()), "vkEnumerateInstanceExtensionProperties");
        for (const auto* name : instanceExtensions) {
            require(name != nullptr, "null instance extension");
            if (std::none_of(available.begin(), available.end(), [&](const auto& item) { return std::strcmp(item.extensionName, name) == 0; })) {
                throw std::runtime_error(std::string("Vulkan presentation: required instance extension missing: ") + name);
            }
        }
        if (std::any_of(available.begin(), available.end(), [](const auto& item) { return std::strcmp(item.extensionName, VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME) == 0; })) {
            instanceExtensions.push_back(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
            state->swapchainColorSpaces = true;
        }
        create.enabledExtensionCount = static_cast<std::uint32_t>(instanceExtensions.size());
        create.ppEnabledExtensionNames = instanceExtensions.data();
    }
    check(state->InstanceFunction<PFN_vkCreateInstance>("vkCreateInstance")(&create, nullptr, &state->instance), "vkCreateInstance");
    APS5_LOG_OUT("Vulkan instance created instance=%p", reinterpret_cast<void*>(state->instance));
    if (window != nullptr) {
        state->surface = window->createSurface(window->context, state->instance);
        require(state->surface != VK_NULL_HANDLE, "window returned a null surface");
        state->window = window->context;
        APS5_LOG_OUT("Vulkan surface created surface=%p window=%p", reinterpret_cast<void*>(state->surface), state->window);
    }
    state->deviceProc = state->InstanceFunction<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");
    const auto enumerate = state->InstanceFunction<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices");
    std::uint32_t count = 0;
    check(enumerate(state->instance, &count, nullptr), "vkEnumeratePhysicalDevices");
    APS5_LOG_OUT("Physical device count=%u", count);
    std::vector<VkPhysicalDevice> devices(count);
    check(enumerate(state->instance, &count, devices.data()), "vkEnumeratePhysicalDevices");
    devices.resize(count);
    VkPhysicalDevice selected = VK_NULL_HANDLE;
    std::uint32_t family = 0;
    int selectedRank = -1;
    const char* requestedName = std::getenv("ANYPS5_GPU");
    if (requestedName != nullptr && *requestedName == '\0') requestedName = nullptr;
    const auto matchesRequest = [&](const char* deviceName) {
        const std::string_view name(deviceName);
        const std::string_view request(requestedName);
        if (request.size() > name.size()) return false;
        for (std::size_t start = 0; start + request.size() <= name.size(); ++start) {
            std::size_t index = 0;
            while (index < request.size() && std::tolower(static_cast<unsigned char>(name[start + index])) == std::tolower(static_cast<unsigned char>(request[index]))) ++index;
            if (index == request.size()) return true;
        }
        return false;
    };
    std::string candidateNames;
    const auto rankDeviceType = [](VkPhysicalDeviceType type) {
        switch (type) {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 3;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 2;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return 1;
            default: return 0;
        }
    };
    const std::array<const char*, 1> presentationExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    for (auto physical : devices) {
        VkPhysicalDeviceProperties properties{};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties")(physical, &properties);
        APS5_LOG_OUT("Physical device candidate=%p name=%s api=0x%x type=%d", reinterpret_cast<void*>(physical), properties.deviceName, properties.apiVersion, static_cast<int>(properties.deviceType));
        if (!candidateNames.empty()) candidateNames += ", ";
        candidateNames += properties.deviceName;
        if (properties.apiVersion < VK_API_VERSION_1_1) {
            continue;
        }
        if (requestedName != nullptr && !matchesRequest(properties.deviceName)) continue;
        const int rank = requestedName != nullptr ? 4 : rankDeviceType(properties.deviceType);
        if (rank <= selectedRank) {
            continue;
        }
        if (window != nullptr) {
            std::uint32_t extensionCount = 0;
            auto enumerateExtensions = state->InstanceFunction<PFN_vkEnumerateDeviceExtensionProperties>("vkEnumerateDeviceExtensionProperties");
            check(enumerateExtensions(physical, nullptr, &extensionCount, nullptr), "vkEnumerateDeviceExtensionProperties");
            std::vector<VkExtensionProperties> extensions(extensionCount);
            check(enumerateExtensions(physical, nullptr, &extensionCount, extensions.data()), "vkEnumerateDeviceExtensionProperties");
            const bool supported = std::all_of(presentationExtensions.begin(), presentationExtensions.end(), [&](const char* name) {
                return std::any_of(extensions.begin(), extensions.end(), [&](const auto& item) { return std::strcmp(item.extensionName, name) == 0; });
            });
            if (!supported) continue;
        }
        std::uint32_t families = 0;
        auto getFamilies = state->InstanceFunction<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");
        getFamilies(physical, &families, nullptr);
        std::vector<VkQueueFamilyProperties> queues(families);
        getFamilies(physical, &families, queues.data());
        for (std::uint32_t i = 0; i < families; ++i) {
            if (queues[i].queueCount != 0 && (queues[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) == (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
                if (window != nullptr) {
                    VkBool32 supported = VK_FALSE;
                    check(state->InstanceFunction<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>("vkGetPhysicalDeviceSurfaceSupportKHR")(physical, i, state->surface, &supported), "vkGetPhysicalDeviceSurfaceSupportKHR");
                    if (!supported) continue;
                }
                selected = physical;
                family = i;
                selectedRank = rank;
                break;
            }
        }
    }
    if (selected == VK_NULL_HANDLE && requestedName != nullptr) {
        throw std::runtime_error(std::string("Vulkan: no usable device whose name contains ANYPS5_GPU=\"") + requestedName + "\"; devices: " + candidateNames);
    }
    if (selected == VK_NULL_HANDLE) {
        throw std::runtime_error(window ? "Vulkan: no Vulkan 1.1 device with graphics, compute and swapchain presentation" : "Vulkan: no Vulkan 1.1 graphics and compute queue");
    }
    APS5_LOG_OUT("Physical device selected physical=%p queueFamily=%u", reinterpret_cast<void*>(selected), family);
    VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    properties.pNext = &state->subgroup;
    state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &properties);
    state->properties = properties.properties;
    state->physical = selected;
    state->srgbDecodeFormats = Graphics::SrgbDecodeFormats(state->InstanceFunction<PFN_vkGetPhysicalDeviceFormatProperties>("vkGetPhysicalDeviceFormatProperties"), selected);
    state->srgbCopyFormats = Graphics::SrgbCopyFormats(state->InstanceFunction<PFN_vkGetPhysicalDeviceFormatProperties>("vkGetPhysicalDeviceFormatProperties"), selected, state->srgbDecodeFormats);
    if ((state->subgroup.supportedOperations & VK_SUBGROUP_FEATURE_BASIC_BIT) != 0) {
        state->capabilities.push_back(spv::CapabilityGroupNonUniform);
        if ((state->subgroup.supportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT) != 0) state->capabilities.push_back(spv::CapabilityGroupNonUniformBallot);
        if ((state->subgroup.supportedOperations & VK_SUBGROUP_FEATURE_SHUFFLE_BIT) != 0) state->capabilities.push_back(spv::CapabilityGroupNonUniformShuffle);
        if ((state->subgroup.supportedOperations & VK_SUBGROUP_FEATURE_ARITHMETIC_BIT) != 0) state->capabilities.push_back(spv::CapabilityGroupNonUniformArithmetic);
    }
    APS5_LOG_OUT("Selected GPU name=%s vendor=0x%x device=0x%x subgroup=%u", state->properties.deviceName, state->properties.vendorID, state->properties.deviceID, state->subgroup.subgroupSize);
    state->InstanceFunction<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(selected, &state->memoryProperties);
    for (std::uint32_t i = 0; i < state->memoryProperties.memoryHeapCount; ++i) APS5_LOG_OUT("Memory heap %u size=%.1f MiB flags=0x%x", i, state->memoryProperties.memoryHeaps[i].size / 1048576.0, state->memoryProperties.memoryHeaps[i].flags);
    for (std::uint32_t i = 0; i < state->memoryProperties.memoryTypeCount; ++i) APS5_LOG_OUT("Memory type %u heap=%u flags=0x%x", i, state->memoryProperties.memoryTypes[i].heapIndex, state->memoryProperties.memoryTypes[i].propertyFlags);
    std::uint32_t extensionCount = 0;
    const auto enumerateDeviceExtensions = state->InstanceFunction<PFN_vkEnumerateDeviceExtensionProperties>("vkEnumerateDeviceExtensionProperties");
    check(enumerateDeviceExtensions(selected, nullptr, &extensionCount, nullptr), "vkEnumerateDeviceExtensionProperties");
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    check(enumerateDeviceExtensions(selected, nullptr, &extensionCount, availableExtensions.data()), "vkEnumerateDeviceExtensionProperties");
    const auto hasExtension = [&](const char* name) { return std::any_of(availableExtensions.begin(), availableExtensions.end(), [&](const auto& item) { return std::strcmp(item.extensionName, name) == 0; }); };
    VkDriverId driverId{};
    if (hasExtension(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME)) {
        VkPhysicalDeviceDriverProperties driver{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
        VkPhysicalDeviceProperties2 driverProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &driver};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &driverProperties);
        driverId = driver.driverID;
        state->driverInfo = std::string(driver.driverName) + " " + driver.driverInfo;
        state->driverId = driver.driverID;
        state->individuallyFreeDescriptorSets = driver.driverID == VK_DRIVER_ID_MESA_LLVMPIPE;
        state->fusedExactArithmetic = driver.driverID == VK_DRIVER_ID_MOLTENVK;
        state->subgroupControlBarriers = SubgroupControlBarrierUsable(driver.driverID);
        state->workgroupScopeFences = WorkgroupScopeFenceUsable(driver.driverID);
    }
    if (driverId == VK_DRIVER_ID_NVIDIA_PROPRIETARY && hasExtension(VK_NV_SHADER_SM_BUILTINS_EXTENSION_NAME)) {
        VkPhysicalDeviceShaderSMBuiltinsPropertiesNV smBuiltins{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SM_BUILTINS_PROPERTIES_NV};
        VkPhysicalDeviceProperties2 smProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &smBuiltins};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &smProperties);
        const auto subgroup = state->subgroup.subgroupSize;
        if (smBuiltins.shaderSMCount != 0u && smBuiltins.shaderWarpsPerSM != 0u && subgroup != 0u) {
            const auto invocations = static_cast<std::uint64_t>(smBuiltins.shaderSMCount) * smBuiltins.shaderWarpsPerSM * subgroup;
            const auto budgetDwords = (4ull << 30) / (invocations * 4ull);
            if (budgetDwords < state->pixelFunctionStorageDwords) state->pixelFunctionStorageDwords = static_cast<std::uint32_t>(budgetDwords);
        }
    }
    auto byteFeatures = QueryBdaByteFeatures(selected, state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2"), availableExtensions);
    auto bdaFeatures = QueryBdaFeatures(selected, state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2"), availableExtensions);
    require(hasExtension(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME), "VK_KHR_shader_float_controls is unavailable");
    VkPhysicalDeviceFloatControlsProperties floatControls{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES};
    VkPhysicalDeviceProperties2 floatProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &floatControls};
    state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &floatProperties);
    require(floatControls.shaderSignedZeroInfNanPreserveFloat32 == VK_TRUE, "shaderSignedZeroInfNanPreserveFloat32 is unavailable");
    const std::array<const char*, 2> meshExtensions{VK_EXT_MESH_SHADER_EXTENSION_NAME, VK_KHR_SPIRV_1_4_EXTENSION_NAME};
    const bool meshAvailable = std::all_of(meshExtensions.begin(), meshExtensions.end(), hasExtension);
    VkPhysicalDeviceMeshShaderFeaturesEXT meshFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
    if (meshAvailable) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &meshFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        state->meshShader = meshFeatures.meshShader == VK_TRUE;
        VkPhysicalDeviceProperties2 meshProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &state->meshLimits};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &meshProperties);
    }
    meshFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
    meshFeatures.meshShader = state->meshShader;
    VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR barycentricFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR};
    if (hasExtension(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME)) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &barycentricFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        state->fragmentShaderBarycentric = barycentricFeatures.fragmentShaderBarycentric == VK_TRUE;
        if (state->fragmentShaderBarycentric && hasExtension(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME)) {
            VkPhysicalDeviceDriverProperties driver{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
            VkPhysicalDeviceProperties2 driverProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &driver};
            state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &driverProperties);
            state->fragmentShaderBarycentric = FragmentShaderBarycentricUsable(driver.driverID, state->fragmentShaderBarycentric);
        }
    }
    VkPhysicalDeviceShaderTerminateInvocationFeaturesKHR terminateFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_TERMINATE_INVOCATION_FEATURES_KHR};
    if (hasExtension(VK_KHR_SHADER_TERMINATE_INVOCATION_EXTENSION_NAME)) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &terminateFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        state->shaderTerminateInvocation = terminateFeatures.shaderTerminateInvocation == VK_TRUE;
    }
    VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT interlockFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT};
    if (hasExtension(VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME)) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &interlockFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        state->fragmentShaderPixelInterlock = interlockFeatures.fragmentShaderPixelInterlock == VK_TRUE;
    }
    interlockFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT};
    interlockFeatures.fragmentShaderPixelInterlock = VK_TRUE;
    VkPhysicalDeviceShaderClockFeaturesKHR clockFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR};
    if (hasExtension(VK_KHR_SHADER_CLOCK_EXTENSION_NAME)) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &clockFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        state->shaderClock = clockFeatures.shaderSubgroupClock == VK_TRUE && clockFeatures.shaderDeviceClock == VK_TRUE;
    }
    if (state->shaderClock && hasExtension(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME)) {
        VkPhysicalDeviceDriverProperties driver{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
        VkPhysicalDeviceProperties2 driverProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &driver};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &driverProperties);
        state->narrowSubgroupClock = NarrowSubgroupClock(driver.driverID, state->properties.deviceID, state->properties.deviceName);
        APS5_LOG_OUT("Shader clock driver=%d narrowSubgroupClock=%d", static_cast<int>(driver.driverID), state->narrowSubgroupClock ? 1 : 0);
    }
    VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT demoteFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES_EXT};
    if (hasExtension(VK_EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION_EXTENSION_NAME)) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &demoteFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        state->demoteToHelperInvocation = demoteFeatures.shaderDemoteToHelperInvocation == VK_TRUE;
    }
    demoteFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES_EXT};
    demoteFeatures.shaderDemoteToHelperInvocation = VK_TRUE;
    if (hasExtension(VK_KHR_MAINTENANCE_5_EXTENSION_NAME)) {
        VkPhysicalDeviceMaintenance5PropertiesKHR maintenance5{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_PROPERTIES_KHR};
        VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &maintenance5};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &properties);
        state->earlyFragmentSampleMaskTestBeforeSampleCounting = maintenance5.earlyFragmentSampleMaskTestBeforeSampleCounting == VK_TRUE;
    }
    std::vector<const char*> deviceExtensions;
    if (window != nullptr) deviceExtensions.assign(presentationExtensions.begin(), presentationExtensions.end());
    if (window != nullptr && hasExtension(VK_EXT_HDR_METADATA_EXTENSION_NAME)) {
        deviceExtensions.push_back(VK_EXT_HDR_METADATA_EXTENSION_NAME);
        state->hdrMetadata = true;
    }
    if (state->fragmentShaderBarycentric) {
        deviceExtensions.push_back(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME);
        state->capabilities.push_back(spv::CapabilityFragmentBarycentricKHR);
        state->spirvExtensions.push_back("SPV_KHR_fragment_shader_barycentric");
    }
    if (state->shaderTerminateInvocation) {
        deviceExtensions.push_back(VK_KHR_SHADER_TERMINATE_INVOCATION_EXTENSION_NAME);
        state->spirvExtensions.push_back("SPV_KHR_terminate_invocation");
    }
    if (state->fragmentShaderPixelInterlock) {
        deviceExtensions.push_back(VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME);
        state->capabilities.push_back(spv::CapabilityFragmentShaderPixelInterlockEXT);
        state->spirvExtensions.push_back("SPV_EXT_fragment_shader_interlock");
    }
    if (hasExtension(VK_EXT_SHADER_STENCIL_EXPORT_EXTENSION_NAME)) {
        deviceExtensions.push_back(VK_EXT_SHADER_STENCIL_EXPORT_EXTENSION_NAME);
        state->capabilities.push_back(spv::CapabilityStencilExportEXT);
        state->spirvExtensions.push_back("SPV_EXT_shader_stencil_export");
        state->shaderStencilExport = true;
    }
    if (state->shaderClock) {
        deviceExtensions.push_back(VK_KHR_SHADER_CLOCK_EXTENSION_NAME);
        state->capabilities.push_back(spv::CapabilityShaderClockKHR);
        state->spirvExtensions.push_back("SPV_KHR_shader_clock");
    }
    VkPhysicalDeviceShaderAtomicInt64FeaturesKHR atomicInt64Features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES_KHR};
    if (hasExtension(VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME)) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &atomicInt64Features};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
    }
    const bool bufferInt64Atomics = atomicInt64Features.shaderBufferInt64Atomics == VK_TRUE;
    if (bufferInt64Atomics) state->capabilities.push_back(spv::CapabilityInt64Atomics);
    atomicInt64Features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES_KHR};
    atomicInt64Features.shaderBufferInt64Atomics = VK_TRUE;
    if (bufferInt64Atomics) deviceExtensions.push_back(VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME);
    state->bufferInt64Atomics = bufferInt64Atomics;
    VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT imageAtomicInt64Features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT};
    if (hasExtension(VK_EXT_SHADER_IMAGE_ATOMIC_INT64_EXTENSION_NAME)) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &imageAtomicInt64Features};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
    }
    const bool imageInt64Atomics = imageAtomicInt64Features.shaderImageInt64Atomics == VK_TRUE;
    imageAtomicInt64Features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT};
    imageAtomicInt64Features.shaderImageInt64Atomics = VK_TRUE;
    if (imageInt64Atomics) {
        deviceExtensions.push_back(VK_EXT_SHADER_IMAGE_ATOMIC_INT64_EXTENSION_NAME);
        state->capabilities.push_back(spv::CapabilityInt64ImageEXT);
        state->imageInt64Atomics = true;
        state->spirvExtensions.push_back("SPV_EXT_shader_image_int64");
    }
    if (hasExtension(VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME)) {
        deviceExtensions.push_back(VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME);
        state->capabilities.push_back(spv::CapabilityShaderViewportIndexLayerEXT);
        state->spirvExtensions.push_back("SPV_EXT_shader_viewport_index_layer");
        state->viewportIndexLayer = true;
    }
    if (hasExtension(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME)) {
        deviceExtensions.push_back(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME);
        state->imageFormatList = true;
    }
    if (state->demoteToHelperInvocation) {
        deviceExtensions.push_back(VK_EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION_EXTENSION_NAME);
        state->capabilities.push_back(spv::CapabilityDemoteToHelperInvocation);
        state->spirvExtensions.push_back("SPV_EXT_demote_to_helper_invocation");
    }
    deviceExtensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
    state->capabilities.push_back(spv::CapabilitySignedZeroInfNanPreserve);
    state->spirvExtensions.push_back("SPV_KHR_float_controls");
    VkPhysicalDeviceShaderFloat16Int8FeaturesKHR float16Features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR};
    if (hasExtension(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME)) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &float16Features};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
    }
    const bool float16Rte = float16Features.shaderFloat16 == VK_TRUE && floatControls.shaderRoundingModeRTEFloat16 == VK_TRUE &&
        floatControls.shaderDenormPreserveFloat16 == VK_TRUE && floatControls.shaderSignedZeroInfNanPreserveFloat16 == VK_TRUE;
    float16Features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR};
    float16Features.shaderFloat16 = VK_TRUE;
    if (float16Rte) {
        deviceExtensions.push_back(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME);
        state->capabilities.push_back(spv::CapabilityFloat16);
        state->capabilities.push_back(spv::CapabilityRoundingModeRTE);
        state->capabilities.push_back(spv::CapabilityDenormPreserve);
    }
    state->float16DenormPreserve = floatControls.shaderDenormPreserveFloat16 == VK_TRUE &&
        floatControls.denormBehaviorIndependence == VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_ALL;
    const auto float32Flush = Float32FlushControlFor(driverId, floatControls.shaderDenormFlushToZeroFloat32, floatControls.denormBehaviorIndependence);
    state->float32FlushToZeroCandidate = float32Flush.probe && std::getenv("APS5_NO_HW_F32_FLUSH") == nullptr;
    if (state->float32FlushToZeroCandidate && float32Flush.declared) state->capabilities.push_back(spv::CapabilityDenormFlushToZero);
    state->float32DenormPreserve = floatControls.shaderDenormPreserveFloat32 == VK_TRUE && floatControls.denormBehaviorIndependence != VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_NONE && std::getenv("APS5_NO_HW_F32_PRESERVE") == nullptr;
    if ((state->float16DenormPreserve || state->float32DenormPreserve) && std::find(state->capabilities.begin(), state->capabilities.end(), static_cast<std::uint32_t>(spv::CapabilityDenormPreserve)) == state->capabilities.end()) state->capabilities.push_back(spv::CapabilityDenormPreserve);
    deviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_8BIT_STORAGE_EXTENSION_NAME);
    state->capabilities.push_back(4448);
    state->spirvExtensions.push_back("SPV_KHR_8bit_storage");
    state->capabilities.push_back(11);
    state->capabilities.push_back(5347);
    state->spirvExtensions.push_back("SPV_KHR_physical_storage_buffer");
    state->depthRangeUnrestricted = hasExtension(VK_EXT_DEPTH_RANGE_UNRESTRICTED_EXTENSION_NAME);
    if (state->depthRangeUnrestricted) deviceExtensions.push_back(VK_EXT_DEPTH_RANGE_UNRESTRICTED_EXTENSION_NAME);
    if (hasExtension(VK_EXT_SAMPLE_LOCATIONS_EXTENSION_NAME)) {
        VkPhysicalDeviceSampleLocationsPropertiesEXT locations{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLE_LOCATIONS_PROPERTIES_EXT};
        VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &locations};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &properties);
        if (locations.sampleLocationSubPixelBits >= 4 && locations.sampleLocationCoordinateRange[0] <= 0.0f && locations.sampleLocationCoordinateRange[1] >= 15.0f / 16.0f) {
            const auto multisampleProperties = state->InstanceFunction<PFN_vkGetPhysicalDeviceMultisamplePropertiesEXT>("vkGetPhysicalDeviceMultisamplePropertiesEXT");
            std::string grids;
            for (const auto samples : {VK_SAMPLE_COUNT_1_BIT, VK_SAMPLE_COUNT_2_BIT, VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_8_BIT}) {
                if ((locations.sampleLocationSampleCounts & samples) == 0) continue;
                VkMultisamplePropertiesEXT multisample{VK_STRUCTURE_TYPE_MULTISAMPLE_PROPERTIES_EXT};
                multisampleProperties(selected, samples, &multisample);
                const auto grid = multisample.maxSampleLocationGridSize;
                state->sampleLocationGridSizes[static_cast<std::size_t>(std::countr_zero(static_cast<std::uint32_t>(samples)))] = grid;
                if (grid.width != 0 && grid.height != 0) state->sampleLocationSampleCounts |= samples;
                if (AgcDriver::Graphics::HostsSampleLocationGrid(grid)) state->sampleLocationGridSampleCounts |= samples;
                grids += (grids.empty() ? "" : ", ") + std::to_string(static_cast<std::uint32_t>(samples)) + " samples " + std::to_string(grid.width) + "x" + std::to_string(grid.height);
            }
            if (!grids.empty()) std::fprintf(stderr, "[gpu] largest sample location grids (vkGetPhysicalDeviceMultisamplePropertiesEXT): %s; maxSampleLocationGridSize for every count %ux%u\n", grids.c_str(), locations.maxSampleLocationGridSize.width, locations.maxSampleLocationGridSize.height);
        }
        if (state->sampleLocationSampleCounts != 0) deviceExtensions.push_back(VK_EXT_SAMPLE_LOCATIONS_EXTENSION_NAME);
    }
    const char* fifoLatestReadyExtension = hasExtension(VK_KHR_PRESENT_MODE_FIFO_LATEST_READY_EXTENSION_NAME) ? VK_KHR_PRESENT_MODE_FIFO_LATEST_READY_EXTENSION_NAME : hasExtension("VK_EXT_present_mode_fifo_latest_ready") ? "VK_EXT_present_mode_fifo_latest_ready" : nullptr;
    const char* requestedPresentMode = std::getenv("ANYPS5_PRESENT_MODE");
    VkPhysicalDevicePresentModeFifoLatestReadyFeaturesKHR fifoLatestReadyFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_MODE_FIFO_LATEST_READY_FEATURES_KHR};
    bool fifoLatestReady = false;
    if (requestedPresentMode != nullptr && std::strcmp(requestedPresentMode, "fifo_latest_ready") == 0 && fifoLatestReadyExtension != nullptr) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &fifoLatestReadyFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        fifoLatestReady = fifoLatestReadyFeatures.presentModeFifoLatestReady == VK_TRUE;
        if (fifoLatestReady) deviceExtensions.push_back(fifoLatestReadyExtension);
    }
    // Guest dispatches cover whole thread groups past the edge of small images; robust image access
    // drops those writes instead of faulting the device.
    const bool imageRobustness = hasExtension(VK_EXT_IMAGE_ROBUSTNESS_EXTENSION_NAME);
    if (imageRobustness) deviceExtensions.push_back(VK_EXT_IMAGE_ROBUSTNESS_EXTENSION_NAME);
    if (hasExtension(VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME)) {
        VkPhysicalDeviceSamplerFilterMinmaxPropertiesEXT minmaxProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES_EXT};
        VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &minmaxProperties};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &properties);
        state->samplerFilterMinmax = minmaxProperties.filterMinmaxSingleComponentFormats == VK_TRUE && minmaxProperties.filterMinmaxImageComponentMapping == VK_TRUE;
        if (state->samplerFilterMinmax) deviceExtensions.push_back(VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME);
    }
    if (hasExtension(VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME)) {
        VkPhysicalDeviceConservativeRasterizationPropertiesEXT conservativeProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONSERVATIVE_RASTERIZATION_PROPERTIES_EXT};
        VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &conservativeProperties};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &properties);
        state->conservativeRasterization = conservativeProperties.primitiveOverestimationSize <= 1.0f / 256.0f && conservativeProperties.degenerateTrianglesRasterized == VK_TRUE;
        if (state->conservativeRasterization) deviceExtensions.push_back(VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME);
    }
    // Indirect draws with a GPU-side count (DRAW_INDIRECT_MULTI with count_indirect); a device
    // without it resolves such draws on the CPU.
    state->drawIndirectCount = hasExtension(VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME);
    if (state->drawIndirectCount) deviceExtensions.push_back(VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME);
    state->memoryBudget = hasExtension(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
    // Guest memory is host memory: importing it lets address-based shaders use it in place instead of
    // copying every registered allocation per draw.
    if (hasExtension(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME) && std::getenv("APS5_NO_HOST_IMPORT") == nullptr) {
        VkPhysicalDeviceMaintenance3Properties maintenance3{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES};
        VkPhysicalDeviceDriverProperties driverProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES, &maintenance3};
        VkPhysicalDeviceExternalMemoryHostPropertiesEXT hostProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT, &driverProperties};
        VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &hostProperties};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &properties);
        state->hostImportAlignment = hostProperties.minImportedHostPointerAlignment;
#ifdef _WIN32
        constexpr bool windows = true;
#else
        constexpr bool windows = false;
#endif
        state->drawInputsInPlace = std::getenv("APS5_NO_DRAW_INPUTS_IN_PLACE") == nullptr && (std::getenv("APS5_DRAW_INPUTS_IN_PLACE") != nullptr || DrawInputsInPlaceUsable(driverProperties.driverID, windows));
        if (!compileOnly) Graphics::SetHostImportWindow(8ull << 20u);
        if (!compileOnly) Graphics::SetHostImportMaxBytes(maintenance3.maxMemoryAllocationSize);
#ifdef _WIN32
        // Imported pages are locked; locking is bounded by the process working-set minimum.
        if (const char* value = std::getenv("APS5_WORKING_SET_MIB"); value != nullptr && !compileOnly) {
            const SIZE_T bytes = static_cast<SIZE_T>(std::strtoull(value, nullptr, 10)) << 20u;
            if (!SetProcessWorkingSetSizeEx(GetCurrentProcess(), bytes, bytes * 2, QUOTA_LIMITS_HARDWS_MIN_DISABLE | QUOTA_LIMITS_HARDWS_MAX_DISABLE)) std::fprintf(stderr, "[gpu] SetProcessWorkingSetSizeEx failed: %lu\n", GetLastError());
        }
#endif
        deviceExtensions.push_back(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME);
#ifndef _WIN32
        state->dmaBufImport = driverProperties.driverID != VK_DRIVER_ID_NVIDIA_PROPRIETARY && hasExtension(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME) && hasExtension(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME);
        if (state->dmaBufImport) {
            deviceExtensions.push_back(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
            deviceExtensions.push_back(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME);
        }
#endif
    }
    VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT listRestartFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVE_TOPOLOGY_LIST_RESTART_FEATURES_EXT};
    if (hasExtension(VK_EXT_PRIMITIVE_TOPOLOGY_LIST_RESTART_EXTENSION_NAME)) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &listRestartFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        state->primitiveListRestart = listRestartFeatures.primitiveTopologyListRestart == VK_TRUE;
        if (state->primitiveListRestart) deviceExtensions.push_back(VK_EXT_PRIMITIVE_TOPOLOGY_LIST_RESTART_EXTENSION_NAME);
    }
    listRestartFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVE_TOPOLOGY_LIST_RESTART_FEATURES_EXT};
    listRestartFeatures.primitiveTopologyListRestart = state->primitiveListRestart ? VK_TRUE : VK_FALSE;
    VkPhysicalDeviceProvokingVertexFeaturesEXT provokingFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_FEATURES_EXT};
    if (hasExtension(VK_EXT_PROVOKING_VERTEX_EXTENSION_NAME)) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &provokingFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        state->provokingVertexLast = provokingFeatures.provokingVertexLast == VK_TRUE;
        if (state->provokingVertexLast) {
            VkPhysicalDeviceProvokingVertexPropertiesEXT provokingProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_PROPERTIES_EXT};
            VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &provokingProperties};
            state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &properties);
            state->provokingVertexModePerPipeline = provokingProperties.provokingVertexModePerPipeline == VK_TRUE;
            deviceExtensions.push_back(VK_EXT_PROVOKING_VERTEX_EXTENSION_NAME);
        }
    }
    provokingFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_FEATURES_EXT};
    provokingFeatures.provokingVertexLast = state->provokingVertexLast ? VK_TRUE : VK_FALSE;
    VkPhysicalDeviceImageViewMinLodFeaturesEXT minLodFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_MIN_LOD_FEATURES_EXT};
    if (hasExtension(VK_EXT_IMAGE_VIEW_MIN_LOD_EXTENSION_NAME)) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &minLodFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        state->imageViewMinLod = minLodFeatures.minLod == VK_TRUE;
        if (state->imageViewMinLod) deviceExtensions.push_back(VK_EXT_IMAGE_VIEW_MIN_LOD_EXTENSION_NAME);
    }
    minLodFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_MIN_LOD_FEATURES_EXT};
    minLodFeatures.minLod = VK_TRUE;
    VkPhysicalDeviceMaintenance8FeaturesKHR maintenance8Features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_8_FEATURES_KHR};
    if (hasExtension(VK_KHR_MAINTENANCE_8_EXTENSION_NAME)) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &maintenance8Features};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        state->maintenance8 = maintenance8Features.maintenance8 == VK_TRUE;
        if (state->maintenance8) deviceExtensions.push_back(VK_KHR_MAINTENANCE_8_EXTENSION_NAME);
    }
    maintenance8Features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_8_FEATURES_KHR};
    maintenance8Features.maintenance8 = VK_TRUE;
    VkPhysicalDeviceDepthClipControlFeaturesEXT depthClipFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_CONTROL_FEATURES_EXT};
    if (hasExtension(VK_EXT_DEPTH_CLIP_CONTROL_EXTENSION_NAME)) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &depthClipFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        state->depthClipControl = depthClipFeatures.depthClipControl == VK_TRUE;
        if (state->depthClipControl) deviceExtensions.push_back(VK_EXT_DEPTH_CLIP_CONTROL_EXTENSION_NAME);
    }
    VkPhysicalDeviceNonSeamlessCubeMapFeaturesEXT nonSeamlessCubeFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_NON_SEAMLESS_CUBE_MAP_FEATURES_EXT};
    if (hasExtension(VK_EXT_NON_SEAMLESS_CUBE_MAP_EXTENSION_NAME)) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &nonSeamlessCubeFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        state->nonSeamlessCubeMap = nonSeamlessCubeFeatures.nonSeamlessCubeMap == VK_TRUE;
        if (state->nonSeamlessCubeMap) deviceExtensions.push_back(VK_EXT_NON_SEAMLESS_CUBE_MAP_EXTENSION_NAME);
    }
    nonSeamlessCubeFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_NON_SEAMLESS_CUBE_MAP_FEATURES_EXT};
    nonSeamlessCubeFeatures.nonSeamlessCubeMap = VK_TRUE;
    VkPhysicalDeviceCoverageReductionModeFeaturesNV coverageReductionFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COVERAGE_REDUCTION_MODE_FEATURES_NV};
    if (hasExtension(VK_NV_FRAMEBUFFER_MIXED_SAMPLES_EXTENSION_NAME) && hasExtension(VK_NV_COVERAGE_REDUCTION_MODE_EXTENSION_NAME)) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &coverageReductionFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        if (coverageReductionFeatures.coverageReductionMode == VK_TRUE) {
            const auto combinations = state->InstanceFunction<PFN_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV>("vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV");
            std::uint32_t count = 0;
            check(combinations(selected, &count, nullptr), "vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV");
            std::vector<VkFramebufferMixedSamplesCombinationNV> supported(count, {VK_STRUCTURE_TYPE_FRAMEBUFFER_MIXED_SAMPLES_COMBINATION_NV});
            check(combinations(selected, &count, supported.data()), "vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV");
            for (const auto& combination : supported) {
                if (combination.coverageReductionMode == VK_COVERAGE_REDUCTION_MODE_MERGE_NV && combination.colorSamples == VK_SAMPLE_COUNT_1_BIT && combination.depthStencilSamples == 0) state->mergedCoverageSampleCounts |= combination.rasterizationSamples;
                if (combination.coverageReductionMode == VK_COVERAGE_REDUCTION_MODE_TRUNCATE_NV && combination.colorSamples == VK_SAMPLE_COUNT_1_BIT && combination.depthStencilSamples == combination.rasterizationSamples) state->truncatedCoverageSampleCounts |= combination.rasterizationSamples;
            }
        }
        if (state->mergedCoverageSampleCounts != 0 || state->truncatedCoverageSampleCounts != 0) {
            deviceExtensions.push_back(VK_NV_FRAMEBUFFER_MIXED_SAMPLES_EXTENSION_NAME);
            deviceExtensions.push_back(VK_NV_COVERAGE_REDUCTION_MODE_EXTENSION_NAME);
        }
    } else if (hasExtension(VK_AMD_MIXED_ATTACHMENT_SAMPLES_EXTENSION_NAME)) {
        state->mixedAttachmentSamples = true;
        state->truncatedCoverageSampleCounts = state->properties.limits.framebufferDepthSampleCounts & state->properties.limits.framebufferColorSampleCounts & ~static_cast<VkSampleCountFlags>(VK_SAMPLE_COUNT_1_BIT);
        deviceExtensions.push_back(VK_AMD_MIXED_ATTACHMENT_SAMPLES_EXTENSION_NAME);
    }
    coverageReductionFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COVERAGE_REDUCTION_MODE_FEATURES_NV};
    coverageReductionFeatures.coverageReductionMode = VK_TRUE;
    VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT libraryFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT};
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT dynamicStateFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT};
    VkPhysicalDeviceDynamicRenderingFeaturesKHR renderingFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR};
    constexpr std::array<const char*, 6> libraryExtensions{VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME, VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME, VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME, VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME};
    bool libraryDevice = false;
    if (std::all_of(libraryExtensions.begin(), libraryExtensions.end(), hasExtension)) {
        dynamicStateFeatures.pNext = &renderingFeatures;
        libraryFeatures.pNext = &dynamicStateFeatures;
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &libraryFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        VkPhysicalDeviceGraphicsPipelineLibraryPropertiesEXT libraryProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_PROPERTIES_EXT};
        VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &libraryProperties};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &properties);
        libraryDevice = libraryFeatures.graphicsPipelineLibrary == VK_TRUE && dynamicStateFeatures.extendedDynamicState == VK_TRUE && renderingFeatures.dynamicRendering == VK_TRUE && libraryProperties.graphicsPipelineLibraryFastLinking == VK_TRUE && properties.properties.limits.maxPushConstantsSize >= Graphics::PipelinePushConstantBytes;
        if (libraryDevice) deviceExtensions.insert(deviceExtensions.end(), libraryExtensions.begin(), libraryExtensions.end());
        state->graphicsPipelineLibrary = libraryDevice && std::getenv("APS5_NO_GPL") == nullptr;
        if (state->graphicsPipelineLibrary && hasExtension(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME)) {
            VkPhysicalDeviceDriverProperties driver{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
            VkPhysicalDeviceProperties2 driverProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &driver};
            state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &driverProperties);
            state->pipelineLinkTimeOptimization = PipelineLinkTimeOptimizationUsable(driver.driverID);
        }
    }
    VkPhysicalDeviceMaintenance5FeaturesKHR maintenance5Features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR};
    if (state->graphicsPipelineLibrary && hasExtension(VK_KHR_MAINTENANCE_5_EXTENSION_NAME)) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &maintenance5Features};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        state->maintenance5 = maintenance5Features.maintenance5 == VK_TRUE;
        if (state->maintenance5) deviceExtensions.push_back(VK_KHR_MAINTENANCE_5_EXTENSION_NAME);
    }
    maintenance5Features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR};
    maintenance5Features.maintenance5 = VK_TRUE;
    renderingFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR, nullptr, VK_TRUE};
    libraryFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT, &dynamicStateFeatures, VK_TRUE};
    dynamicStateFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT, &renderingFeatures, VK_TRUE};
    if (state->meshShader) {
        deviceExtensions.insert(deviceExtensions.end(), meshExtensions.begin(), meshExtensions.end());
        state->capabilities.push_back(5283);
        state->spirvExtensions.push_back("SPV_EXT_mesh_shader");
    }
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = family;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;
    const float priorities[2]{1.0f, 1.0f};
#ifdef _WIN32
    const bool settles = true;
#else
    const bool settles = state->dmaBufImport;
#endif
    std::vector<VkQueueFamilyProperties> queues;
    {
        std::uint32_t families = 0;
        auto getFamilies = state->InstanceFunction<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");
        getFamilies(selected, &families, nullptr);
        queues.resize(families);
        getFamilies(selected, &families, queues.data());
    }
    VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    VkPhysicalDeviceFeatures available{};
    state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures>("vkGetPhysicalDeviceFeatures")(selected, &available);
    require(available.vertexPipelineStoresAndAtomics && available.fragmentStoresAndAtomics, "graphics shader buffer writes and atomics are unavailable");
    VkPhysicalDeviceFeatures enabled{};
    enabled.shaderInt64 = VK_TRUE;
    enabled.shaderFloat64 = available.shaderFloat64 && floatControls.shaderSignedZeroInfNanPreserveFloat64;
    if (enabled.shaderFloat64) state->capabilities.push_back(spv::CapabilityFloat64);
    state->float64DenormPreserve = enabled.shaderFloat64 && floatControls.shaderDenormPreserveFloat64 == VK_TRUE &&
        floatControls.denormBehaviorIndependence == VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_ALL;
    if (state->float64DenormPreserve && std::find(state->capabilities.begin(), state->capabilities.end(), static_cast<std::uint32_t>(spv::CapabilityDenormPreserve)) == state->capabilities.end()) state->capabilities.push_back(spv::CapabilityDenormPreserve);
    enabled.vertexPipelineStoresAndAtomics = VK_TRUE;
    enabled.fragmentStoresAndAtomics = VK_TRUE;
    enabled.tessellationShader = available.tessellationShader;
    state->tessellationShader = enabled.tessellationShader == VK_TRUE;
    if (state->tessellationShader) state->capabilities.push_back(3);
    VkPhysicalDeviceShaderDrawParametersFeatures drawParametersFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES};
    {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &drawParametersFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
    }
    const bool drawParameters = drawParametersFeatures.shaderDrawParameters == VK_TRUE;
    drawParametersFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES, nullptr, VK_TRUE};
    if (drawParameters) state->capabilities.push_back(spv::CapabilityDrawParameters);
    APS5_LOG_OUT("Vulkan features tessellationAvailable=%u mesh=%u depthClip=%u depthRangeUnrestricted=%u pipelineLibrary=%u", static_cast<unsigned>(state->tessellationShader), static_cast<unsigned>(state->meshShader), static_cast<unsigned>(state->depthClipControl), static_cast<unsigned>(state->depthRangeUnrestricted), static_cast<unsigned>(state->graphicsPipelineLibrary));
    require(available.samplerAnisotropy && available.textureCompressionBC, "device lacks sampler anisotropy or BC texture compression support required for texture sampling");
    enabled.samplerAnisotropy = VK_TRUE;
    enabled.textureCompressionBC = VK_TRUE;
    // Guest shaders routinely read past descriptor ranges; robust access turns that into zeros
    // instead of a GPU fault that loses the device.
    enabled.robustBufferAccess = available.robustBufferAccess;
    const auto plan = PlanDeviceQueues(queues, family, settles, available.sparseBinding == VK_TRUE);
    const bool settleQueue = plan.settle;
    const auto settleFamily = plan.settleFamily;
    const auto settleIndex = plan.settleIndex;
    VkDeviceQueueCreateInfo queueInfos[3]{queueInfo, {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}, {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}};
    queueInfos[0].queueCount = plan.mainCount;
    queueInfos[0].pQueuePriorities = plan.mainCount == 2 ? priorities : &priority;
    deviceInfo.queueCreateInfoCount = 1;
    const auto addQueue = [&](std::uint32_t queueFamily, std::uint32_t index) {
        for (std::uint32_t info = 1; info < deviceInfo.queueCreateInfoCount; ++info) {
            if (queueInfos[info].queueFamilyIndex != queueFamily) continue;
            queueInfos[info].queueCount = std::max(queueInfos[info].queueCount, index + 1);
            queueInfos[info].pQueuePriorities = queueInfos[info].queueCount == 2 ? priorities : &priority;
            return;
        }
        auto& added = queueInfos[deviceInfo.queueCreateInfoCount++];
        added = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        added.queueFamilyIndex = queueFamily;
        added.queueCount = index + 1;
        added.pQueuePriorities = added.queueCount == 2 ? priorities : &priority;
    };
    if (plan.settle && plan.settleFamily != family) addQueue(plan.settleFamily, plan.settleIndex);
    if (plan.sparseFamily != VK_QUEUE_FAMILY_IGNORED) addQueue(plan.sparseFamily, plan.sparseIndex);
    deviceInfo.pQueueCreateInfos = queueInfos;
    state->sparseFamily = plan.sparseFamily;
    state->sparseIndex = plan.sparseIndex;
    state->sparseBinding = plan.sparseBinding;
    enabled.sparseBinding = state->sparseBinding ? VK_TRUE : VK_FALSE;
    enabled.sparseResidencyBuffer = state->sparseBinding ? available.sparseResidencyBuffer : VK_FALSE;
    enabled.sparseResidencyAliased = state->sparseBinding ? available.sparseResidencyAliased : VK_FALSE;
    state->sparseResidencyBuffer = enabled.sparseResidencyBuffer == VK_TRUE;
    state->sparseResidencyAliased = enabled.sparseResidencyAliased == VK_TRUE;
    // Indirect draw records with a non-zero first instance, and several records per call.
    enabled.drawIndirectFirstInstance = available.drawIndirectFirstInstance;
    enabled.multiDrawIndirect = available.multiDrawIndirect;
    state->drawIndirectFirstInstance = enabled.drawIndirectFirstInstance == VK_TRUE;
    state->multiDrawIndirect = enabled.multiDrawIndirect == VK_TRUE;
    // PA_CL_CLIP_CNTL near/far clip disable maps to depth clamping.
    enabled.depthClamp = available.depthClamp;
    state->depthClamp = enabled.depthClamp == VK_TRUE;
    enabled.fillModeNonSolid = available.fillModeNonSolid;
    state->fillModeNonSolid = enabled.fillModeNonSolid == VK_TRUE;
    enabled.depthBounds = available.depthBounds;
    state->depthBounds = enabled.depthBounds == VK_TRUE;
    enabled.depthBiasClamp = available.depthBiasClamp;
    state->depthBiasClamp = enabled.depthBiasClamp == VK_TRUE;
    enabled.dualSrcBlend = available.dualSrcBlend;
    state->dualSrcBlend = enabled.dualSrcBlend == VK_TRUE;
    enabled.occlusionQueryPrecise = available.occlusionQueryPrecise;
    state->occlusionQueryPrecise = enabled.occlusionQueryPrecise == VK_TRUE;
    // Recompiled storage-image access declares no format (the guest descriptor decides it).
    enabled.shaderStorageImageMultisample = available.shaderStorageImageMultisample;
    state->storageImageMultisample = enabled.shaderStorageImageMultisample == VK_TRUE;
    if (enabled.shaderStorageImageMultisample) state->capabilities.push_back(spv::CapabilityStorageImageMultisample);
    state->capabilities.push_back(spv::CapabilityImageMSArray);
    enabled.shaderStorageImageWriteWithoutFormat = available.shaderStorageImageWriteWithoutFormat;
    state->storageImageWriteWithoutFormat = enabled.shaderStorageImageWriteWithoutFormat == VK_TRUE;
    enabled.shaderStorageImageReadWithoutFormat = available.shaderStorageImageReadWithoutFormat;
    // Gathers with non-constant offsets (ImageGatherExtended).
    enabled.shaderImageGatherExtended = available.shaderImageGatherExtended;
    if (enabled.shaderImageGatherExtended) state->capabilities.push_back(spv::CapabilityImageGatherExtended);
    enabled.shaderResourceMinLod = available.shaderResourceMinLod;
    if (enabled.shaderResourceMinLod) state->capabilities.push_back(spv::CapabilityMinLod);
    enabled.sampleRateShading = available.sampleRateShading;
    enabled.geometryShader = available.geometryShader;
    state->geometryShader = enabled.geometryShader == VK_TRUE;
    enabled.largePoints = available.largePoints;
    state->largePoints = enabled.largePoints == VK_TRUE;
    state->sampleRateShading = enabled.sampleRateShading == VK_TRUE;
    if (enabled.sampleRateShading) state->capabilities.push_back(spv::CapabilityInterpolationFunction);
    if (enabled.geometryShader) state->capabilities.push_back(spv::CapabilityGeometry);
    enabled.shaderClipDistance = available.shaderClipDistance;
    enabled.shaderCullDistance = available.shaderCullDistance;
    state->shaderClipDistance = enabled.shaderClipDistance == VK_TRUE;
    state->shaderCullDistance = enabled.shaderCullDistance == VK_TRUE;
    if (enabled.shaderClipDistance) state->capabilities.push_back(spv::CapabilityClipDistance);
    if (enabled.shaderCullDistance) state->capabilities.push_back(spv::CapabilityCullDistance);
    if (enabled.shaderStorageImageWriteWithoutFormat) state->capabilities.push_back(spv::CapabilityStorageImageWriteWithoutFormat);
    if (enabled.shaderStorageImageReadWithoutFormat) state->capabilities.push_back(spv::CapabilityStorageImageReadWithoutFormat);
    state->storageImageWithoutFormat = enabled.shaderStorageImageWriteWithoutFormat == VK_TRUE && enabled.shaderStorageImageReadWithoutFormat == VK_TRUE;
    // Bindless image tables index an image array with a wave-uniform runtime slot.
    enabled.shaderSampledImageArrayDynamicIndexing = available.shaderSampledImageArrayDynamicIndexing;
    enabled.shaderStorageImageArrayDynamicIndexing = available.shaderStorageImageArrayDynamicIndexing;
    if (enabled.shaderSampledImageArrayDynamicIndexing) state->capabilities.push_back(spv::CapabilitySampledImageArrayDynamicIndexing);
    if (enabled.shaderStorageImageArrayDynamicIndexing) state->capabilities.push_back(spv::CapabilityStorageImageArrayDynamicIndexing);
    VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptorIndexingFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT};
    if (hasExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &descriptorIndexingFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        state->descriptorIndexing = descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing == VK_TRUE && descriptorIndexingFeatures.shaderStorageImageArrayNonUniformIndexing == VK_TRUE;
        state->storageBufferUpdateAfterBind = descriptorIndexingFeatures.descriptorBindingStorageBufferUpdateAfterBind == VK_TRUE;
        state->sampledImageUpdateAfterBind = descriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind == VK_TRUE;
        state->descriptorBindingPartiallyBound = descriptorIndexingFeatures.descriptorBindingPartiallyBound == VK_TRUE;
        state->descriptorBindingVariableDescriptorCount = descriptorIndexingFeatures.descriptorBindingVariableDescriptorCount == VK_TRUE;
        state->runtimeDescriptorArray = descriptorIndexingFeatures.runtimeDescriptorArray == VK_TRUE;
        if (state->storageBufferUpdateAfterBind || state->sampledImageUpdateAfterBind) {
            VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &state->descriptorIndexingProperties};
            state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &properties);
        }
    }
    descriptorIndexingFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT};
    if (state->descriptorIndexing || state->storageBufferUpdateAfterBind || state->sampledImageUpdateAfterBind || state->descriptorBindingPartiallyBound || state->descriptorBindingVariableDescriptorCount || state->runtimeDescriptorArray) deviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
    if (state->descriptorIndexing) {
        descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        descriptorIndexingFeatures.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
        state->capabilities.push_back(spv::CapabilityShaderNonUniform);
        state->capabilities.push_back(spv::CapabilitySampledImageArrayNonUniformIndexing);
        state->capabilities.push_back(spv::CapabilityStorageImageArrayNonUniformIndexing);
        state->spirvExtensions.push_back("SPV_EXT_descriptor_indexing");
    }
    descriptorIndexingFeatures.descriptorBindingStorageBufferUpdateAfterBind = state->storageBufferUpdateAfterBind ? VK_TRUE : VK_FALSE;
    descriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind = state->sampledImageUpdateAfterBind ? VK_TRUE : VK_FALSE;
    descriptorIndexingFeatures.descriptorBindingPartiallyBound = state->descriptorBindingPartiallyBound ? VK_TRUE : VK_FALSE;
    descriptorIndexingFeatures.descriptorBindingVariableDescriptorCount = state->descriptorBindingVariableDescriptorCount ? VK_TRUE : VK_FALSE;
    descriptorIndexingFeatures.runtimeDescriptorArray = state->runtimeDescriptorArray ? VK_TRUE : VK_FALSE;
    if (state->runtimeDescriptorArray) {
        state->capabilities.push_back(spv::CapabilityRuntimeDescriptorArray);
        if (!state->descriptorIndexing) state->spirvExtensions.push_back("SPV_EXT_descriptor_indexing");
    }
    state->samplerAnisotropy = true;
    state->textureCompressionBC = true;
    deviceInfo.pEnabledFeatures = &enabled;
    const char* calibratedTimestamps = !PerformanceTrace::Configured() ? nullptr : hasExtension(VK_KHR_CALIBRATED_TIMESTAMPS_EXTENSION_NAME) ? VK_KHR_CALIBRATED_TIMESTAMPS_EXTENSION_NAME : hasExtension(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME) ? VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME : nullptr;
    if (calibratedTimestamps != nullptr) deviceExtensions.push_back(calibratedTimestamps);
    deviceInfo.enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size());
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
    if (state->meshShader) {
        meshFeatures.pNext = const_cast<void*>(deviceInfo.pNext);
        deviceInfo.pNext = &meshFeatures;
    }
    if (state->depthClipControl) {
        depthClipFeatures.pNext = const_cast<void*>(deviceInfo.pNext);
        deviceInfo.pNext = &depthClipFeatures;
    }
    if (libraryDevice) {
        renderingFeatures.pNext = const_cast<void*>(deviceInfo.pNext);
        deviceInfo.pNext = &libraryFeatures;
    }
    if (state->primitiveListRestart) {
        listRestartFeatures.pNext = const_cast<void*>(deviceInfo.pNext);
        deviceInfo.pNext = &listRestartFeatures;
    }
    if (state->provokingVertexLast) {
        provokingFeatures.pNext = const_cast<void*>(deviceInfo.pNext);
        deviceInfo.pNext = &provokingFeatures;
    }
    if (state->imageViewMinLod) {
        minLodFeatures.pNext = const_cast<void*>(deviceInfo.pNext);
        deviceInfo.pNext = &minLodFeatures;
    }
    fifoLatestReadyFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_MODE_FIFO_LATEST_READY_FEATURES_KHR};
    fifoLatestReadyFeatures.presentModeFifoLatestReady = VK_TRUE;
    if (fifoLatestReady) {
        fifoLatestReadyFeatures.pNext = const_cast<void*>(deviceInfo.pNext);
        deviceInfo.pNext = &fifoLatestReadyFeatures;
    }
    if (state->maintenance5) {
        maintenance5Features.pNext = const_cast<void*>(deviceInfo.pNext);
        deviceInfo.pNext = &maintenance5Features;
    }
    if (state->maintenance8) {
        maintenance8Features.pNext = const_cast<void*>(deviceInfo.pNext);
        deviceInfo.pNext = &maintenance8Features;
    }
    if (state->nonSeamlessCubeMap) {
        nonSeamlessCubeFeatures.pNext = const_cast<void*>(deviceInfo.pNext);
        deviceInfo.pNext = &nonSeamlessCubeFeatures;
    }
    if (!state->mixedAttachmentSamples && (state->mergedCoverageSampleCounts != 0 || state->truncatedCoverageSampleCounts != 0)) {
        coverageReductionFeatures.pNext = const_cast<void*>(deviceInfo.pNext);
        deviceInfo.pNext = &coverageReductionFeatures;
    }
    byteFeatures.pNext = const_cast<void*>(deviceInfo.pNext);
    if (state->fragmentShaderBarycentric) {
        barycentricFeatures.pNext = byteFeatures.pNext;
        byteFeatures.pNext = &barycentricFeatures;
    }
    if (state->shaderTerminateInvocation) {
        terminateFeatures.shaderTerminateInvocation = VK_TRUE;
        terminateFeatures.pNext = byteFeatures.pNext;
        byteFeatures.pNext = &terminateFeatures;
    }
    if (state->fragmentShaderPixelInterlock) {
        interlockFeatures.pNext = byteFeatures.pNext;
        byteFeatures.pNext = &interlockFeatures;
    }
    if (state->shaderClock) {
        clockFeatures.pNext = byteFeatures.pNext;
        byteFeatures.pNext = &clockFeatures;
    }
    if (float16Rte) {
        float16Features.pNext = byteFeatures.pNext;
        byteFeatures.pNext = &float16Features;
    }
    if (bufferInt64Atomics) {
        atomicInt64Features.pNext = byteFeatures.pNext;
        byteFeatures.pNext = &atomicInt64Features;
    }
    if (imageInt64Atomics) {
        imageAtomicInt64Features.pNext = byteFeatures.pNext;
        byteFeatures.pNext = &imageAtomicInt64Features;
    }
    if (state->demoteToHelperInvocation) {
        demoteFeatures.pNext = byteFeatures.pNext;
        byteFeatures.pNext = &demoteFeatures;
    }
    VkPhysicalDeviceImageRobustnessFeaturesEXT imageRobustnessFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES_EXT, nullptr, VK_TRUE};
    if (imageRobustness) {
        imageRobustnessFeatures.pNext = byteFeatures.pNext;
        byteFeatures.pNext = &imageRobustnessFeatures;
    }
    if (state->descriptorIndexing || state->storageBufferUpdateAfterBind || state->sampledImageUpdateAfterBind || state->descriptorBindingPartiallyBound || state->descriptorBindingVariableDescriptorCount || state->runtimeDescriptorArray) {
        descriptorIndexingFeatures.pNext = byteFeatures.pNext;
        byteFeatures.pNext = &descriptorIndexingFeatures;
    }
    if (drawParameters) {
        drawParametersFeatures.pNext = byteFeatures.pNext;
        byteFeatures.pNext = &drawParametersFeatures;
    }
    // Timeline semaphores let a queue worker wait for recorded batches without holding the GPU mutex
    // (see Recorder::WaitSerial). The instance is 1.1, so the KHR extension is used even on 1.2+
    // devices. Debug aid: APS5_NO_TIMELINE=1 leaves it off (drains wait under the mutex as before).
    VkPhysicalDeviceTimelineSemaphoreFeaturesKHR timelineFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR};
    if (hasExtension(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME) && std::getenv("APS5_NO_TIMELINE") == nullptr) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &timelineFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        state->timelineSemaphores = timelineFeatures.timelineSemaphore == VK_TRUE;
    }
    VkPhysicalDeviceSubgroupSizeControlFeaturesEXT subgroupSizeFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT};
    if (hasExtension(VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME) && state->subgroup.subgroupSize >= 32u) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &subgroupSizeFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroupSize{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT};
        VkPhysicalDeviceProperties2 sizeProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &subgroupSize};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &sizeProperties);
        state->computeWave32 = subgroupSizeFeatures.subgroupSizeControl == VK_TRUE && subgroupSize.minSubgroupSize <= 32u && subgroupSize.maxSubgroupSize >= 32u &&
            (subgroupSize.requiredSubgroupSizeStages & VK_SHADER_STAGE_COMPUTE_BIT) != 0;
        state->meshWave32 = state->meshShader && subgroupSizeFeatures.subgroupSizeControl == VK_TRUE && subgroupSize.minSubgroupSize <= 32u && subgroupSize.maxSubgroupSize >= 32u &&
            (subgroupSize.requiredSubgroupSizeStages & VK_SHADER_STAGE_MESH_BIT_EXT) != 0;
        state->maxComputeSubgroupSize = subgroupSize.maxSubgroupSize;
        state->computeFullSubgroups = state->computeWave32 && subgroupSizeFeatures.computeFullSubgroups == VK_TRUE;
    }
    if (state->computeWave32 || state->meshWave32) {
        subgroupSizeFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT};
        subgroupSizeFeatures.subgroupSizeControl = VK_TRUE;
        subgroupSizeFeatures.computeFullSubgroups = state->computeFullSubgroups ? VK_TRUE : VK_FALSE;
        deviceExtensions.push_back(VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME);
        deviceInfo.enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size());
        deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
        subgroupSizeFeatures.pNext = byteFeatures.pNext;
        byteFeatures.pNext = &subgroupSizeFeatures;
        APS5_LOG_OUT("Wave32 programs run on subgroups of %u: compute %d, mesh %d", 32u, state->computeWave32, state->meshWave32);
    }
    if (std::getenv("APS5_SPANNED_WAVE") != nullptr) {
        state->spannedWaves = state->computeFullSubgroups;
        APS5_LOG_OUT("APS5_SPANNED_WAVE: single-wave wave64 compute spans two full 32-lane subgroups: %d", state->spannedWaves);
    }
    timelineFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR};
    timelineFeatures.timelineSemaphore = VK_TRUE;
    if (state->timelineSemaphores) {
        deviceExtensions.push_back(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
        deviceInfo.enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size());
        deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
        timelineFeatures.pNext = byteFeatures.pNext;
        byteFeatures.pNext = &timelineFeatures;
    } else {
        std::fprintf(stderr, "[gpu] timeline semaphores unavailable or disabled; drains wait under the GPU mutex\n");
    }
    VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR pipelineFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR};
    const auto pipelineStats = std::getenv("APS5_PIPELINE_STATS");
    if (pipelineStats && std::string_view(pipelineStats) == "1") {
        Graphics::Require(hasExtension(VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_EXTENSION_NAME), "APS5_PIPELINE_STATS requires VK_KHR_pipeline_executable_properties");
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &pipelineFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        Graphics::Require(pipelineFeatures.pipelineExecutableInfo == VK_TRUE, "APS5_PIPELINE_STATS requires pipelineExecutableInfo");
        state->pipelineExecutableInfo = true;
        deviceExtensions.push_back(VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_EXTENSION_NAME);
        deviceInfo.enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size());
        deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
        pipelineFeatures.pNext = byteFeatures.pNext;
        byteFeatures.pNext = &pipelineFeatures;
    }
    VkPhysicalDeviceFaultFeaturesEXT faultFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT};
    if (hasExtension(VK_EXT_DEVICE_FAULT_EXTENSION_NAME)) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &faultFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
    }
    const bool deviceFault = faultFeatures.deviceFault == VK_TRUE;
    if (deviceFault) {
        faultFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT};
        faultFeatures.deviceFault = VK_TRUE;
        deviceExtensions.push_back(VK_EXT_DEVICE_FAULT_EXTENSION_NAME);
        deviceInfo.enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size());
        deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
        faultFeatures.pNext = byteFeatures.pNext;
        byteFeatures.pNext = &faultFeatures;
    }
    const bool checkpointsRequested = std::getenv("APS5_GPU_CHECKPOINTS") != nullptr;
    const bool checkpoints = checkpointsRequested && hasExtension(VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME);
    const bool bufferMarkers = checkpointsRequested && !checkpoints && hasExtension(VK_AMD_BUFFER_MARKER_EXTENSION_NAME);
    if (checkpoints || bufferMarkers) {
        deviceExtensions.push_back(checkpoints ? VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME : VK_AMD_BUFFER_MARKER_EXTENSION_NAME);
        deviceInfo.enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size());
        deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
    }
    bdaFeatures.pNext = &byteFeatures;
    VkPhysicalDeviceRobustness2FeaturesEXT robustness2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT};
    if (hasExtension(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME)) {
        VkPhysicalDeviceFeatures2 queried{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &robustness2};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &queried);
    }
    state->robustBufferAccess2 = robustness2.robustBufferAccess2 == VK_TRUE && enabled.robustBufferAccess == VK_TRUE;
    if (robustness2.nullDescriptor == VK_TRUE || state->robustBufferAccess2) {
        const bool nullDescriptor = robustness2.nullDescriptor == VK_TRUE;
        robustness2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT};
        robustness2.nullDescriptor = nullDescriptor ? VK_TRUE : VK_FALSE;
        robustness2.robustBufferAccess2 = state->robustBufferAccess2 ? VK_TRUE : VK_FALSE;
        robustness2.pNext = bdaFeatures.pNext;
        bdaFeatures.pNext = &robustness2;
        deviceExtensions.push_back(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME);
        deviceInfo.enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size());
        deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
    }
    deviceInfo.pNext = &bdaFeatures;
    auto shaderProfile = std::make_unique<const ShaderDeviceProfile>(buildTarget(), deviceInfo, state->properties.limits);
    check(state->InstanceFunction<PFN_vkCreateDevice>("vkCreateDevice")(selected, &deviceInfo, nullptr, &state->device), "vkCreateDevice");
    state->shaderProfile = std::move(shaderProfile);
    if (compileOnly) return;
    if (calibratedTimestamps != nullptr) {
        const bool khr = std::strcmp(calibratedTimestamps, VK_KHR_CALIBRATED_TIMESTAMPS_EXTENSION_NAME) == 0;
        const auto domains = reinterpret_cast<PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR>(state->instanceProc(state->instance, khr ? "vkGetPhysicalDeviceCalibrateableTimeDomainsKHR" : "vkGetPhysicalDeviceCalibrateableTimeDomainsEXT"));
        std::uint32_t domainCount = 0;
        std::vector<VkTimeDomainKHR> available;
        if (domains != nullptr && domains(selected, &domainCount, nullptr) == VK_SUCCESS) {
            available.resize(domainCount);
            if (domains(selected, &domainCount, available.data()) != VK_SUCCESS) available.clear();
        }
#ifdef _WIN32
        const auto host = VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR;
#else
        const auto host = VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR;
#endif
        if (std::ranges::find(available, VK_TIME_DOMAIN_DEVICE_KHR) != available.end() && std::ranges::find(available, host) != available.end()) {
            state->hostTimeDomain = host;
            state->calibratedTimestamps = state->DeviceFunction<PFN_vkGetCalibratedTimestampsKHR>(khr ? "vkGetCalibratedTimestampsKHR" : "vkGetCalibratedTimestampsEXT");
        }
    }
    state->DeviceFunction<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(state->device, family, 0, &state->queue);
    state->queueFamily = family;
    if (settleQueue) {
        state->settleFamily = settleFamily;
        state->DeviceFunction<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(state->device, settleFamily, settleIndex, &state->settleQueue);
    }
    if (state->sparseFamily != VK_QUEUE_FAMILY_IGNORED) state->DeviceFunction<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(state->device, state->sparseFamily, state->sparseIndex, &state->sparseQueue);
    if (checkpoints) state->checkpointMarker = state->DeviceFunction<PFN_vkCmdSetCheckpointNV>("vkCmdSetCheckpointNV");
    if (bufferMarkers) state->CreateMarkerBuffer();
    if (deviceFault || checkpoints || bufferMarkers) {
        const auto faultInfo = deviceFault ? state->DeviceFunction<PFN_vkGetDeviceFaultInfoEXT>("vkGetDeviceFaultInfoEXT") : nullptr;
        const auto checkpointData = checkpoints ? state->DeviceFunction<PFN_vkGetQueueCheckpointDataNV>("vkGetQueueCheckpointDataNV") : nullptr;
        Graphics::DeviceLostHook() = [device = state->device, queue = state->queue, faultInfo, checkpointData, owner = state.get()] {
            if (owner->markerMapping != nullptr) {
                std::uint32_t words[2];
                std::memcpy(words, owner->markerMapping, sizeof(words));
                for (const auto& line : owner->markerLog.Report(words[0], words[1])) std::fprintf(stderr, "%s\n", line.c_str());
            }
            for (const auto& line : DeviceFaultReport(device, faultInfo, queue, checkpointData)) std::fprintf(stderr, "%s\n", line.c_str());
        };
        Graphics::DeviceLostOwner() = state.get();
        state->deviceLostHook = true;
    }
    APS5_LOG_OUT("Vulkan device ready device=%p queue=%p family=%u", reinterpret_cast<void*>(state->device), reinterpret_cast<void*>(state->queue), family);
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = family;
    check(state->DeviceFunction<PFN_vkCreateCommandPool>("vkCreateCommandPool")(state->device, &poolInfo, nullptr, &state->pool), "vkCreateCommandPool");
    if (state->settleQueue != VK_NULL_HANDLE) {
        VkCommandPoolCreateInfo settlePoolInfo = poolInfo;
        settlePoolInfo.queueFamilyIndex = state->settleFamily;
        check(state->DeviceFunction<PFN_vkCreateCommandPool>("vkCreateCommandPool")(state->device, &settlePoolInfo, nullptr, &state->settlePool), "vkCreateCommandPool settle");
    }
    static const bool procTable = std::getenv("APS5_NO_PROC_TABLE") == nullptr;
    if (procTable) {
        Graphics::FillDeviceFunctions(graphicsContext(), state->deviceFunctions);
        state->functionsReady = true;
    }
    Graphics::PrepareImportWatch(graphicsContext());
    state->bufferPool = std::make_shared<Graphics::BufferPool>(graphicsContext());
    state->emptyBuffer = std::make_unique<Graphics::Buffer>(graphicsContext(), Graphics::EmptyBufferBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    state->pipelineCache = std::make_unique<Graphics::PipelineCache>(graphicsContext(), state->properties, window != nullptr || compileOnly);
    state->detiler = std::make_unique<Graphics::TextureDetiler>(graphicsContext());
    state->textureCache = std::make_unique<Graphics::TextureCache>(graphicsContext());
    state->colorTransfer = std::make_unique<Graphics::GpuColorTransfer>(graphicsContext());
    state->descriptorCache = std::make_unique<Graphics::DescriptorCache>(graphicsContext());
    state->vertexInputs = std::make_unique<Graphics::VertexInputCache>(graphicsContext());
    state->samplerCache = std::make_unique<Graphics::SamplerCache>();
    state->CreateWorkgroupMemoryLayout();
    if (!state->shaderProfile->NullDescriptors()) {
        std::unique_lock gpuLock(GuestMemory::GpuMutex(), std::defer_lock);
        if (!GuestMemory::GpuMutex().HeldByThisThread()) gpuLock.lock();
        state->placeholderImages = std::make_unique<Graphics::PlaceholderImages>(graphicsContext());
    }
    state->recorder = std::make_unique<Graphics::Recorder>(graphicsContext(), state->timelineSemaphores);
    state->recorder->Activate();
    state->context = buildContext();
    state->contextReady = true;
    if (window != nullptr) {
        require(window->getDrawableSize != nullptr, "missing window drawable size query");
        std::uint32_t drawableWidth = 0;
        std::uint32_t drawableHeight = 0;
        window->getDrawableSize(window->context, &drawableWidth, &drawableHeight);
        require(drawableWidth != 0 && drawableHeight != 0, "window has a zero drawable size at creation");
        VkSurfaceCapabilitiesKHR surface{};
        check(state->InstanceFunction<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>("vkGetPhysicalDeviceSurfaceCapabilitiesKHR")(selected, state->surface, &surface), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
        state->extent = {drawableWidth, drawableHeight};
        APS5_LOG_OUT("Surface capabilities drawable=%ux%u min=%ux%u max=%ux%u minImages=%u maxImages=%u usage=0x%x", drawableWidth, drawableHeight, surface.minImageExtent.width, surface.minImageExtent.height, surface.maxImageExtent.width, surface.maxImageExtent.height, surface.minImageCount, surface.maxImageCount, surface.supportedUsageFlags);
        // A surface that reports its extent (a fullscreen transition can make it differ from the
        // drawable size for a moment) wins: the swapchain must match the surface.
        if (surface.currentExtent.width != std::numeric_limits<std::uint32_t>::max() && surface.currentExtent.width != 0 && surface.currentExtent.height != 0) {
            drawableWidth = surface.currentExtent.width;
            drawableHeight = surface.currentExtent.height;
            state->extent = {drawableWidth, drawableHeight};
        }
        require(drawableWidth >= surface.minImageExtent.width && drawableWidth <= surface.maxImageExtent.width && drawableHeight >= surface.minImageExtent.height && drawableHeight <= surface.maxImageExtent.height, "unsupported output extent");
        require((surface.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0, "surface does not support transfer destination images");
        require((surface.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) != 0, "opaque composition is unavailable");
        require((surface.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0, "identity surface transform is unavailable");
        refreshSurfaceFormats();
        SetPresentingHdr10(false);
        require(std::any_of(state->surfaceFormats.begin(), state->surfaceFormats.end(), [](const auto& format) { return format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; }), "BGRA8 sRGB-nonlinear surface format is unavailable");
        std::uint32_t presentModeCount = 0;
        auto getPresentModes = state->InstanceFunction<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>("vkGetPhysicalDeviceSurfacePresentModesKHR");
        check(getPresentModes(selected, state->surface, &presentModeCount, nullptr), "vkGetPhysicalDeviceSurfacePresentModesKHR");
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        check(getPresentModes(selected, state->surface, &presentModeCount, presentModes.data()), "vkGetPhysicalDeviceSurfacePresentModesKHR");
        presentModes.resize(presentModeCount);
        state->presentMode = SelectPresentMode(std::getenv("ANYPS5_PRESENT_MODE"), presentModes);
        require((surface.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0, "surface does not support color attachment images");
        VkSwapchainCreateInfoKHR swapchain{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        swapchain.surface = state->surface;
        swapchain.minImageCount = surface.minImageCount;
        swapchain.imageFormat = state->surfaceFormat;
        swapchain.imageColorSpace = state->surfaceColorSpace;
        swapchain.imageExtent = state->extent;
        swapchain.imageArrayLayers = 1;
        swapchain.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchain.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchain.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        swapchain.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchain.presentMode = state->presentMode;
        swapchain.clipped = VK_FALSE;
        check(state->DeviceFunction<PFN_vkCreateSwapchainKHR>("vkCreateSwapchainKHR")(state->device, &swapchain, nullptr, &state->swapchain), "vkCreateSwapchainKHR");
        std::uint32_t imageCount = 0;
        auto getImages = state->DeviceFunction<PFN_vkGetSwapchainImagesKHR>("vkGetSwapchainImagesKHR");
        check(getImages(state->device, state->swapchain, &imageCount, nullptr), "vkGetSwapchainImagesKHR");
        state->images.resize(imageCount);
        check(getImages(state->device, state->swapchain, &imageCount, state->images.data()), "vkGetSwapchainImagesKHR");
        APS5_LOG_OUT("Swapchain created swapchain=%p extent=%ux%u images=%u", reinterpret_cast<void*>(state->swapchain), state->extent.width, state->extent.height, imageCount);
        VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        const auto createFence = state->DeviceFunction<PFN_vkCreateFence>("vkCreateFence");
        check(createFence(state->device, &fence, nullptr, &state->acquireFence), "vkCreateFence");
        state->images.resize(imageCount);
        state->rendered.resize(imageCount, VK_NULL_HANDLE);
        VkCommandBufferAllocateInfo allocation{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocation.commandPool = state->pool;
        allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocation.commandBufferCount = 1;
        state->presentSlots.resize(FlipInFlight() + 1);
        for (auto& slot : state->presentSlots) {
            check(createFence(state->device, &fence, nullptr, &slot.fence), "vkCreateFence present");
            check(state->DeviceFunction<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(state->device, &allocation, &slot.commands), "vkAllocateCommandBuffers");
        }
        state->scaler = std::make_unique<PresentationScaler>(graphicsContext(), VK_FORMAT_B8G8R8A8_UNORM, state->surfaceFormat);
        state->presentation = std::make_unique<PresentationPass>(graphicsContext(), state->presentSlots.size());
    }
    if (state->float32FlushToZeroCandidate) {
        const auto probeStart = std::chrono::steady_clock::now();
        state->probingFloatControls = true;
        const auto probe = ProbeFloat32FlushToZero(*this, Target());
        state->probingFloatControls = false;
        state->float32FlushToZero = probe.mismatches == 0u;
        std::fprintf(stderr, "[float-controls] f32 denormal flush: %s (%u of %u probe results differ from the emulated flush, %.0f ms)\n", state->float32FlushToZero ? "hardware" : "emulated", probe.mismatches, probe.compared, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - probeStart).count());
    }
    // A device without a window is usually replaced by the window's device before the title draws
    // (Driver::AttachWindow): its replay starts at its first draw or dispatch instead, so the
    // journal is replayed once, on the device the title draws with.
    state->journalReplayPending.store(!compileOnly, std::memory_order_release);
    if (window != nullptr) startJournalReplay();
}

void VulkanDevice::startJournalReplay() {
    if (!state->journalReplayPending.exchange(false, std::memory_order_acq_rel)) return;
    state->journalReplay = Graphics::StartPipelineJournalReplay(graphicsContext(), Graphics::PipelineJournalDeviceFingerprint(state->properties), {[this](const ShaderRecompiler::RecompileResult& shader, bool pushConstants) { return ReplayComputePipeline(shader, pushConstants); }});
}

VulkanDevice::~VulkanDevice() = default;

VkPipelineCreateFlags VulkanDevice::computePipelineFlags() const {
    return (graphicsContext().pipelineExecutableInfo ? VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR : 0) | VK_PIPELINE_CREATE_DISPATCH_BASE_BIT;
}

std::uint32_t VulkanDevice::computeSubgroupSize(const ShaderRecompiler::RecompileResult& shader) const {
    return state->computeWave32 && (shader.hostSubgroupSize == 32u || shader.hostSubgroupSize > state->maxComputeSubgroupSize) ? std::min(shader.hostSubgroupSize, state->maxComputeSubgroupSize) : 0u;
}

VkPipelineShaderStageCreateFlags VulkanDevice::computeStageFlags(const ShaderRecompiler::RecompileResult& shader) const {
    if (!shader.fullSubgroups) return 0;
    require(state->computeFullSubgroups && computeSubgroupSize(shader) == 32u, "a spanned wave64 program needs full 32-lane compute subgroups");
    return VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT;
}

bool VulkanDevice::ComputeFullSubgroups() const {
    return state->computeFullSubgroups;
}

void VulkanDevice::WaitForPipelineJournalReplay() {
    startJournalReplay();
    if (state->journalReplay != nullptr) state->journalReplay->Wait();
}

void VulkanDevice::SavePipelineCache() {
    if (state->pipelineCache) state->pipelineCache->SaveNow();
}

std::uint64_t VulkanDevice::ComputePipelinesCreated() const {
    return state->computePipelinesCreated.load(std::memory_order_relaxed);
}

bool VulkanDevice::ReplayComputePipeline(const ShaderRecompiler::RecompileResult& shader, bool pushConstants) {
    ShaderRecompiler::RuntimeAbi::RequireVersion(shader.runtimeAbiVersion);
    const auto key = ComputePipelineKey::Of(shader, pushConstants);
    if (key.identity.Empty()) return false;
    {
        std::lock_guard pipelines(state->computePipelinesMutex);
        if (state->computePipelines.contains(key) || state->computePipelinesBuilding.contains(key)) return false;
    }
    const auto context = graphicsContext();
    const std::array<Graphics::CompiledShader, 1> shaders{{{ShaderRecompiler::ShaderStage::Compute, &shader, 0}}};
    const auto layouts = Graphics::DescribeSetLayouts(context, shaders);
    auto objects = createComputePipeline(context, shader, layouts.layouts.main, layouts.mainKey, pushConstants ? VK_SHADER_STAGE_COMPUTE_BIT : 0u, 0);
    std::lock_guard pipelines(state->computePipelinesMutex);
    return state->computePipelines.try_emplace(key, std::move(objects)).second;
}

bool VulkanDevice::ImportGuestMemory(const GuestAllocations::Mapped& ranges, std::uint64_t generation, bool adoptDevice) {
    if (adoptDevice) GuestMemory::AssertGpuLockHeld("VulkanDevice::ImportGuestMemory");
    return Graphics::ImportMappedRanges(graphicsContext(), ranges, generation, adoptDevice);
}

void VulkanDevice::PrepareForReplacement() {
    GuestMemory::AssertGpuLockHeld("VulkanDevice::PrepareForReplacement");
    require(state->recorder != nullptr && Graphics::Recorder::Active() == state->recorder.get(), "device replacement requires its active recorder");
    state->journalReplayPending.store(false, std::memory_order_release);
    if (state->journalReplay != nullptr) {
        state->journalReplay->Cancel();
        state->journalReplay->Wait();
        state->journalReplay.reset();
    }
    DriverDetail::AwaitLibraryPrebuilds();
    Graphics::FlushCachedTextures(state->device);
    Graphics::PublishAllShadows(state->context, Graphics::PublishReason::Teardown);
    WaitIdle();
    Graphics::DestroyShadows(state->device);
    // The replacement's draws never find this device's pipelines (the store keys on the device), and
    // the device lives on until the driver stops: destroy them now rather than then.
    Graphics::ClearCachedPipelines(state->device);
    std::lock_guard pipelines(state->computePipelinesMutex);
    state->computePipelines.clear();
}

void VulkanDevice::WaitIdle() {
    APS5_LOG_CHARS_OUT_DEBUG("VulkanDevice::WaitIdle begin");
    RetirePresents(0);
    if (state->recorder) {
        // Announced with this call's return address: the [recorder] site table then names the
        // driver site that drains (suspend point, label fallback, fill fallback, device replacement).
        if (!state->recorder->Idle()) Graphics::Recorder::CountSync(0, __builtin_return_address(0));
        state->recorder->Sync();
    }
    check(state->DeviceFunction<PFN_vkDeviceWaitIdle>("vkDeviceWaitIdle")(state->device), "vkDeviceWaitIdle");
    Graphics::CheckPixelFunctionLds(graphicsContext());
    APS5_LOG_CHARS_OUT_DEBUG("VulkanDevice::WaitIdle complete");
}

namespace {

bool OpportunisticReap() {
    static const bool enabled = std::getenv("APS5_NO_OPPORTUNISTIC_REAP") == nullptr;
    return enabled;
}

}

void VulkanDevice::StoreIdleResults(bool reapFirst) {
    if (!Graphics::StorageTexture::AnyPending()) return;
    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Flush);
    std::lock_guard gpuLock(GuestMemory::GpuMutex());
    const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
    Graphics::StorageTexture::FlushAllPending("idle");
    SubmitRecorded(reapFirst);
}

void VulkanDevice::SubmitRecorded(bool reapFirst) {
    if (!state->recorder) return;
    // Finished batches are retired first (a non-blocking fence status check on the front of the
    // in-flight list), so PendingWriteOverlaps/HasCompletions scans stay short and a label recorded
    // after a completing batch already finished can go to the GPU instead of behind a completion.
    // Not for cross-queue submitters: a retired batch's write-back may sync a later batch under the
    // mutex (see the header), which a poller or compute worker must never do for queue 0.
    if (reapFirst && OpportunisticReap()) state->recorder->Reap();
    state->recorder->Submit();
}

bool VulkanDevice::CanWaitUnlocked() const {
    return state->recorder && state->recorder->HasTimeline();
}

std::uint64_t VulkanDevice::SubmitAndEpoch() {
    if (!state->recorder) return 0;
    if (OpportunisticReap()) state->recorder->Reap();
    return state->recorder->SubmitAndEpoch();
}

void VulkanDevice::WaitRecorded(std::uint64_t serial) {
    if (state->recorder) state->recorder->WaitSerial(serial);
}

void VulkanDevice::ReapRecorded(std::uint64_t serial) {
    if (!state->recorder) return;
    // The drain's site in the [recorder] table is this call's caller (Driver::execute's drain,
    // PrepareDispatch's presync), not this wrapper; FinishUpTo consumes the announcement.
    Graphics::Recorder::AnnounceSyncSite(__builtin_return_address(0));
    state->recorder->FinishUpTo(serial);
}

void VulkanDevice::ReapRecorded() {
    if (state->recorder) state->recorder->Reap();
}

std::optional<Graphics::Recorder::LabelHit> VulkanDevice::PendingLabel(std::uint64_t address, std::size_t bytes, std::uint64_t afterStamp, Graphics::Recorder::LabelRefusal* refusal) const {
    if (!state->recorder) return std::nullopt;
    return state->recorder->PendingLabel(address, bytes, afterStamp, refusal);
}

bool VulkanDevice::OpenWriteOverlaps(std::uint64_t address, std::size_t bytes) const {
    return state->recorder && state->recorder->OpenWriteOverlaps(address, bytes);
}

void VulkanDevice::SealPassWriteOnly() {
    if (state->recorder) state->recorder->SealPassWriteOnly();
}

bool VulkanDevice::RecordedWritesSettled(std::uint64_t address, std::size_t bytes) const {
    if (!state->recorder) return true;
    const auto& recorder = *state->recorder;
    if (!recorder.PendingWriteSettled(address, bytes)) return false;
    return Graphics::Recorder::PendingCompletionLabels() == 0 || !recorder.CompletionLabelIn(address, bytes);
}

bool VulkanDevice::StoresPendingOver(std::uint64_t address, std::size_t bytes) const {
    const std::array<std::pair<std::uint64_t, std::uint64_t>, 1> range{{{address, address + bytes}}};
    if (Graphics::StorageTexture::AnyPendingOverlaps(range) || Graphics::AnyShadowedOverlaps(range) || state->CopiedWriterOverlaps(address, bytes)) return true;
    return state->recorder && (state->recorder->PendingWriteOverlaps(address, bytes) || state->recorder->PendingLabelIn(address, bytes));
}

int VulkanDevice::WriteLabelOnGpu(std::uint64_t address, std::span<const std::byte> bytes, std::uint64_t stamp, std::uint32_t queue, bool reapFirst) {
    static const bool storeAtLabel = Graphics::StorageTexture::StoreAtLabelRequested(std::getenv("APS5_STORE_AT_LABEL"));
    if (storeAtLabel) Graphics::StorageTexture::FlushAllPending("label");
    Graphics::StorageTexture::StoreReadbackBeforeLabel("readback before label");
    if (!state->recorder || bytes.empty() || bytes.size() > 65536 || address % 4 != 0 || bytes.size() % 4 != 0) return 4;
    auto& recorder = *state->recorder;
    // Batches that already finished are retired before the checks: their completions ran, so they
    // neither keep the recorder busy nor force this label behind a completion. Only for the
    // graphics worker: a reap runs completions (copied write-backs, whose guest stores can re-enter
    // the flush hook and sync a later batch) under the mutex, which a compute worker must never do
    // against queue 0; its label goes behind a completion instead (reason 5, still no wait).
    // Debug aid: APS5_LABEL_REAP_ALL_QUEUES=1 reaps on every queue as before.
    static const bool reapAllQueues = std::getenv("APS5_LABEL_REAP_ALL_QUEUES") != nullptr;
    if (reapFirst && (queue == 0 || reapAllQueues) && OpportunisticReap()) recorder.Reap();
    if (recorder.Idle()) return 1;
    static const bool drain = std::getenv("APS5_DRAIN_COMPLETION_LABELS") != nullptr;
    const auto context = graphicsContext();
    const auto* import = Graphics::HostImportFor(context, address, bytes.size());
    if (import == nullptr) return 3;
    // A unit shadow's results in the label's unit reach the import before the store lands over
    // part of it (the store's stamp then makes the unit stale); the deferred label paths call no
    // FlushPending of their own.
    if (Graphics::AnyShadowedOverlaps(address, bytes.size())) Graphics::PublishShadow(address, bytes.size(), Graphics::PublishScope::PartialUnits, Graphics::PublishReason::Label);
    // The store on the GPU, into the open batch: everything recorded so far completes before it,
    // and it is visible to the host after. The barriers belong to the recorder's store run: one
    // pair for a whole group of labels recorded back to back (Recorder::RecordStore orders the
    // stores of a run against each other and against the work before and after, the queued DCC
    // key stores included).
    const auto recordStore = [&] { recorder.RecordStore(import->buffer, Graphics::HostImportOffset(*import, address), bytes, address); };
    // On the console every store a queue made before a label is in memory once the label is
    // visible. A listed copied writer (a dispatch or draw whose written buffer the device holds as a
    // CPU copy) is written back by the CPU when its batch is reaped, after the GPU already ran any
    // store recorded here, so a title polling the label could rewrite the written region and have
    // the late write-back land over its bytes (a title that rebuilds its command buffer there sees
    // a stale packet). While one is listed, a label of 8 bytes or less (every RELEASE_MEM, the fence
    // WRITE_DATAs) is therefore not recorded on the GPU at all: a completion action of the open (or
    // newest in-flight) batch stores it when that batch is reaped. Completions run in push order and
    // batches finish front-first, so it lands after every earlier write-back, whatever range that
    // one wrote. The GPU store plus completion re-store below would not do: its GPU store is
    // visible before the write-back, and its re-store runs only over a write-back that overlapped
    // the label. While such a completion store is pending (PendingCompletionStores: no end-of-pipe
    // delivery and no write-back counts there), later small labels follow it even after every
    // writer was written back, or a GPU store would become visible before the earlier completion
    // store lands (#1458), or land under it at the reap when both are at one address. Larger stores
    // (DUMP_CONST_RAM, COPY_DATA and DMA_DATA payloads that later GPU work reads through the import)
    // stay on the GPU and follow only a pending completion store over their own range. Readers of a
    // range stored this way sync through its batch first (the label is a pending write of its batch
    // and counted in PendingCompletionLabels). Debug aid: APS5_DRAIN_COMPLETION_LABELS=1 drains
    // (reason 2) instead and stores the label on the CPU.
    const bool listed = !state->copiedWriters->empty() || !Graphics::DrawCopiedWriters()->empty();
    if (drain) {
        if (listed) return 2;
    } else if (const bool storesPending = Graphics::Recorder::PendingCompletionStores() != 0; bytes.size() <= 8 ? (listed || storesPending) : (storesPending && recorder.CompletionLabelIn(address, bytes.size()))) {
        recorder.AfterCompletions(address, bytes, stamp, queue, false);
        return 5;
    }
    // A larger store over a listed copied writer's range (as FillBuffer tests it) goes behind the
    // completions too, below; any other one stores on the GPU. Debug aid: APS5_LABEL_GATE_ALL=1
    // gates on any completion, as before (a completion label there registered its own completion,
    // so every later label followed).
    static const bool gateAll = std::getenv("APS5_LABEL_GATE_ALL") != nullptr;
    if (gateAll ? recorder.HasCompletions() : state->CopiedWriterOverlaps(address, bytes.size())) {
        // A copied buffer's write-back is still to run: the store must land after it. It is stored
        // by a completion action of the batch (a CPU memcpy when the batch is reaped) instead of
        // draining the device here. The same bytes are also recorded on the GPU into the open batch
        // first, so GPU work recorded after this point that reads the range through the host import
        // (a shader binding constants a DUMP_CONST_RAM dumped, a dispatch consuming a COPY_DATA or
        // DMA_DATA fill) sees them in order; the completion memcpy stores identical bytes again
        // after any write-back that overwrote them, so either order of the two stores is correct.
        // CPU readers are covered by the pending-write note (the flush hook syncs) and waits by the
        // label table. Debug aid: APS5_DRAIN_COMPLETION_LABELS=1 drains as before; try it first when
        // a title misbehaves with this path.
        if (drain) return 2;
        recordStore();
        // Commands() opened the batch the store went into, so the completion is appended to that
        // batch and runs at its reap, after the write-backs of every batch before it.
        recorder.AfterCompletions(address, bytes, stamp, queue, true);
        GuestMemory::MarkWritten(address, bytes.size());
        return 5;
    }
    recordStore();
    // The table entry and the label's own pending-write note (Recorder::NoteLabel).
    recorder.NoteLabel(address, bytes, stamp, queue);
    GuestMemory::MarkWritten(address, bytes.size());
    // No submit per label: the batch goes out at the queue worker's next non-label packet, at a
    // wait on its range, after APS5_LABEL_FLUSH_US, or at the submission's end (Driver.cpp).
    // Debug aid: APS5_LABEL_SUBMIT_NOW=1 submits every label at once, as before.
    static const bool submitNow = std::getenv("APS5_LABEL_SUBMIT_NOW") != nullptr;
    if (submitNow) recorder.Submit();
    return 0;
}

bool VulkanDevice::AfterRecordedWork(std::function<void()> action, bool reapFirst) {
    if (!state->recorder) return false;
    if (reapFirst && OpportunisticReap()) state->recorder->Reap();
    return state->recorder->AfterRecordedWork(std::move(action));
}

bool VulkanDevice::DumpSamplesOnGpu(std::uint64_t address) {
    static const bool syncDumps = std::getenv("APS5_SYNC_SAMPLE_DUMPS") != nullptr || std::getenv("APS5_NO_GPU_SAMPLE_DUMPS") != nullptr;
    if (syncDumps) return false;
    if (!state->recorder) return false;
    auto& recorder = *state->recorder;
    constexpr std::size_t bytes = Pm4::SampleDumpBytes;
    if (OpportunisticReap()) recorder.Reap();
    if (state->CopiedWriterOverlaps(address, bytes) || (Graphics::Recorder::PendingCompletionLabels() != 0 && recorder.CompletionLabelIn(address, bytes))) return false;
    const auto context = graphicsContext();
    Graphics::StorageTexture::FlushPending(address, bytes, nullptr, "occlusion counter dump", Graphics::PublishScope::PartialUnits);
    const auto* import = Graphics::HostImportFor(context, address, bytes);
    if (import == nullptr || import->address == 0) return false;
    if (Graphics::AnyShadowedOverlaps(address, bytes)) Graphics::PublishShadow(address, bytes, Graphics::PublishScope::PartialUnits, Graphics::PublishReason::Label);
    if (!recorder.QueuesSampleDumps()) recorder.FlushStoresOverlapping(address, bytes);
    recorder.FlushKeyStoresOverlapping(address, bytes);
    if (!recorder.DumpSamples(import->address + (address - import->base), address)) return false;
    recorder.NotePendingWrite(address, bytes);
    GuestMemory::MarkWritten(address, bytes);
    return true;
}

bool VulkanDevice::FillBuffer(std::uint64_t address, std::size_t bytes, std::span<const std::uint32_t, 4> pattern) {
    if (!state->recorder || bytes == 0 || bytes % 16 != 0 || address % 16 != 0) return false;
    auto& recorder = *state->recorder;
    // Finished batches are retired first (one fence status check, as WriteLabelOnGpu does): their
    // completion labels landed and their copied writers are delisted, so a batch the idle GPU already
    // finished does not force a wait below. Before the import lookup: a completion may refresh the
    // import table. Debug aid: APS5_NO_OPPORTUNISTIC_REAP=1 skips it.
    if (OpportunisticReap()) recorder.Reap();
    const auto context = graphicsContext();
    const Graphics::HostImport* import = Graphics::HostImportFor(context, address, bytes);
    if (import == nullptr) return false;
    // Earlier recorded work that stores into the range must land before the fill, or it would
    // overwrite the fill later. Stores the GPU makes (direct dispatch writes, GPU labels, earlier
    // fills, GPU-direct write-backs) are ordered by the fill's own barrier below (ALL_COMMANDS ->
    // TRANSFER), so only the stores the CPU makes when a batch is reaped need a wait: a recorded
    // dispatch whose copied buffer overlaps the range (state->copiedWriters, listed until its
    // write-back ran; per object over all its written V# ranges, as DispatchIndirect tests it) and a
    // completion-deferred label (Recorder::AfterCompletions) over the range. The label table has no
    // range query and a fill spans MiBs, so a small range is looked up per dword (a GPU label there
    // counts too: a harmless extra wait) and a larger one waits whenever any completion label is
    // pending at all. Batches recorded after the last such store keep running (SyncThrough).
    // Debug aid: APS5_FILL_SYNC=1 waits for every recorded store over the range, as before.
    static const bool alwaysSync = std::getenv("APS5_FILL_SYNC") != nullptr;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    // APS5_PROFILE_DRAW: the fills' outcomes, cumulative, every 10 s (under the GpuMutex like the
    // rest) on a [fill-sync] line; Driver.cpp's [fill] line counts the fills and their bytes.
    // `labelSynced` counts syncs taken because a completion label was pending: over the range for
    // a small fill, anywhere at all for a larger one.
    static std::uint64_t fills = 0, synced = 0, labelSynced = 0, ordered = 0;
    static double syncedMs = 0;
    // The [fill-sync] line's phases (APS5_PROFILE_DRAW): decide (the reap, the import lookup and
    // the pending-write tests up to the record), record (barriers and the fill or its doubling
    // chain), notes (the pending-write note and the write stamp); fills by uniform dword vs
    // 16-byte pattern, with the pattern fills' doubling steps.
    static double decideMs = 0, recordMs = 0, notesMs = 0;
    static std::uint64_t uniformFills = 0, patternFills = 0, doublingSteps = 0, patternBuffersReused = 0, patternBuffersMade = 0;
    static auto lastReport = std::chrono::steady_clock::now();
    // A 16-byte pattern is copied from a device-local buffer holding it repeated (filled once by a
    // doubling chain in device memory, cached per pattern), one transfer into the import per fill.
    // Debug aid: APS5_FILL_CHAIN=1 seeds and doubles the pattern in place in the import, as before.
    static const bool chainInPlace = std::getenv("APS5_FILL_CHAIN") != nullptr;
    constexpr std::size_t PatternBufferBytes = 16u << 20u;
    constexpr std::size_t PatternBuffersKept = 8;
    const auto fillStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    ++fills;
    if (recorder.PendingWriteOverlaps(address, bytes)) {
        // Recorded draws with copied writes keep their own registry (Graphics::DrawCopiedWriters):
        // their CPU write-back would land after a fill recorded on the GPU, so they count too.
        bool sync = alwaysSync || state->CopiedWriterOverlaps(address, bytes);
        if (!sync && Graphics::Recorder::PendingCompletionLabels() != 0) {
            // A label the CPU will store after a completion must not be overwritten by the fill:
            // only a label inside the filled range matters, whatever the range's size.
            sync = recorder.PendingLabelIn(address, bytes);
            if (sync) ++labelSynced;
        }
        if (sync) {
            ++synced;
            const auto syncStart = std::chrono::steady_clock::now();
            Graphics::Recorder::CountSync(4);
            recorder.SyncThrough(address, bytes);
            syncedMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - syncStart).count();
            // The sync ran completions, whose write-backs can refresh the import table and retire
            // the import looked up above; the caller stores the fill itself when it is gone.
            import = Graphics::HostImportFor(context, address, bytes);
            if (import == nullptr) return false;
        } else {
            ++ordered;
        }
    }
    if (profile) {
        const auto now = std::chrono::steady_clock::now();
        if (now - lastReport > std::chrono::seconds(10)) {
            lastReport = now;
            AgcDriver::ProfilePrint_nid_no_patch("[fill-sync] fills (cumulative): %llu synced (%llu with a completion label pending) waited %.0f ms, %llu ordered by barrier, %llu with no pending write; phases ms: decide %.0f record %.0f notes %.0f; %llu uniform, %llu pattern (%llu doubling steps in place; pattern buffers reused %llu, made %llu)\n", static_cast<unsigned long long>(synced), static_cast<unsigned long long>(labelSynced), syncedMs, static_cast<unsigned long long>(ordered), static_cast<unsigned long long>(fills - synced - ordered), decideMs, recordMs, notesMs, static_cast<unsigned long long>(uniformFills), static_cast<unsigned long long>(patternFills), static_cast<unsigned long long>(doublingSteps), static_cast<unsigned long long>(patternBuffersReused), static_cast<unsigned long long>(patternBuffersMade));
        }
    }
    auto phaseStart = fillStart;
    const auto phase = [&](double& total) {
        if (!profile) return;
        const auto now = std::chrono::steady_clock::now();
        total += std::chrono::duration<double, std::milli>(now - phaseStart).count();
        phaseStart = now;
    };
    phase(decideMs);
    using CommandClass = Graphics::Recorder::CommandClass;
    // A queued DCC key store over the range must land before the fill.
    recorder.FlushKeyStoresOverlapping(address, bytes);
    const bool uniform = pattern[0] == pattern[1] && pattern[1] == pattern[2] && pattern[2] == pattern[3];
    const std::array<std::uint32_t, 4> patternWords{pattern[0], pattern[1], pattern[2], pattern[3]};
    std::shared_ptr<Graphics::DeviceBuffer> patternBuffer;
    if (!uniform && !chainInPlace) {
        auto& buffers = state->patternBuffers;
        const auto found = std::find_if(buffers.begin(), buffers.end(), [&](const auto& entry) { return entry.first == patternWords; });
        if (found != buffers.end()) {
            patternBuffer = found->second;
            std::rotate(found, std::next(found), buffers.end());
            ++patternBuffersReused;
        } else {
            patternBuffer = std::make_shared<Graphics::DeviceBuffer>(context, PatternBufferBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            auto seed = std::make_shared<Graphics::Buffer>(context, 16, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            std::memcpy(seed->Bytes().data(), pattern.data(), 16);
            recorder.Keep(seed);
            const auto chain = recorder.Commands();
            const auto copyBuffer = context.Resolved(&Graphics::DeviceFunctions::cmdCopyBuffer, "vkCmdCopyBuffer");
            const VkBufferCopy first{0, 0, 16};
            copyBuffer(chain, seed->Handle(), patternBuffer->Handle(), 1, &first);
            for (std::size_t done = 16; done < PatternBufferBytes; done *= 2) {
                Graphics::RecordMemoryBarrier(context, chain, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
                const VkBufferCopy copy{0, done, done};
                copyBuffer(chain, patternBuffer->Handle(), patternBuffer->Handle(), 1, &copy);
            }
            Graphics::RecordMemoryBarrier(context, chain, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            if (buffers.size() >= PatternBuffersKept) buffers.erase(buffers.begin());
            buffers.emplace_back(patternWords, patternBuffer);
            ++patternBuffersMade;
        }
        // The buffer outlives its use in this batch even when evicted from the cache meanwhile.
        recorder.Keep(patternBuffer);
    }
    // A queued label store over the range lands before the fill (program order).
    recorder.FlushStoresOverlapping(address, bytes);
    VkAccessFlags covered = 0;
    const auto commands = recorder.Commands(&covered);
    const auto timing = recorder.BeginGpuTiming(CommandClass::Fill);
    if (Graphics::Recorder::BarrierValidate()) {
        const std::pair<std::uint64_t, std::uint64_t> range{address, address + bytes};
        recorder.NoteAccess(CommandClass::Fill, Graphics::Recorder::Access{{}, std::span(&range, 1), {}, VK_PIPELINE_STAGE_TRANSFER_BIT});
    }
    // Every earlier recorded read or write of the range precedes the fill (WAR by the execution
    // dependency, WAW by the writes made available), whichever stage made it; the previous
    // command's trailing barrier already did that when it covered transfer writes.
    if ((covered & VK_ACCESS_TRANSFER_WRITE_BIT) != 0 && Graphics::Recorder::MergeBarriers()) {
        Graphics::Recorder::CountMerged(CommandClass::Fill);
    } else {
        Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        Graphics::Recorder::CountBarriers(CommandClass::Fill);
    }
    const auto offset = Graphics::HostImportOffset(*import, address);
    if (uniform) {
        ++uniformFills;
        context.Resolved(&Graphics::DeviceFunctions::cmdFillBuffer, "vkCmdFillBuffer")(commands, import->buffer, offset, bytes, pattern[0]);
    } else if (patternBuffer != nullptr) {
        ++patternFills;
        std::vector<VkBufferCopy> copies;
        for (std::size_t done = 0; done < bytes; done += PatternBufferBytes) copies.push_back({0, offset + done, std::min(PatternBufferBytes, bytes - done)});
        context.Resolved(&Graphics::DeviceFunctions::cmdCopyBuffer, "vkCmdCopyBuffer")(commands, patternBuffer->Handle(), import->buffer, static_cast<std::uint32_t>(copies.size()), copies.data());
    } else {
        ++patternFills;
        // A 16-byte pattern is seeded once and doubled in place until the range is covered.
        auto seed = std::make_shared<Graphics::Buffer>(context, 16, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        std::memcpy(seed->Bytes().data(), pattern.data(), 16);
        recorder.Keep(seed);
        const VkBufferCopy first{0, offset, 16};
        const auto copyBuffer = context.Resolved(&Graphics::DeviceFunctions::cmdCopyBuffer, "vkCmdCopyBuffer");
        copyBuffer(commands, seed->Handle(), import->buffer, 1, &first);
        for (std::size_t done = 16; done < bytes;) {
            const auto chunk = std::min(done, bytes - done);
            Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            Graphics::Recorder::CountBarriers(CommandClass::Fill);
            const VkBufferCopy copy{offset, offset + done, chunk};
            copyBuffer(commands, import->buffer, import->buffer, 1, &copy);
            done += chunk;
            ++doublingSteps;
        }
    }
    // The filled bytes are visible to everything recorded after: shaders, transfers, the host, and
    // an indirect dispatch reading its group counts in place (DispatchIndirect adds its own
    // ALL_COMMANDS -> DRAW_INDIRECT barrier as well). A later GPU label store over the range is
    // ordered behind the fill by its ALL_COMMANDS -> TRANSFER barrier (WriteLabelOnGpu).
    constexpr VkAccessFlags filledAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_HOST_READ_BIT;
    Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, filledAccess);
    Graphics::Recorder::CountBarriers(CommandClass::Fill);
    recorder.EndGpuTiming(timing, bytes);
    recorder.MarkCovered(filledAccess);
    phase(recordMs);
    if (uniform) recorder.NotePendingWordFill(address, bytes, pattern[0]);
    else recorder.NotePendingWrite(address, bytes);
    GuestMemory::MarkWritten(address, bytes);
    phase(notesMs);
    return true;
}

namespace {

std::atomic<std::uint64_t> copiesVerified{0}, copySourceChanged{0}, copyDestinationChanged{0}, copyReaderMissed{0};
std::atomic<std::uint64_t> copiesAliased{0}, copyAliasCreated{0}, copyAliasNoSource{0}, copyAliasShape{0}, copyAliasRefused{0};

// The copy as a device copy between the surfaces' storage images (StorageTexture::CopyFrom): the
// source range must be exactly a live image's surface, and the destination range either exactly
// a live image of the same shape or none (one is made from the source's descriptor rebased). Under
// GuestMemory::GpuMutex. Debug aid: APS5_NO_COPY_ALIAS=1 keeps every such copy a transfer.
bool CopyAliasDisabled() {
    static const bool disabled = std::getenv("APS5_NO_COPY_ALIAS") != nullptr;
    return disabled;
}

struct AliasPlan {
    std::shared_ptr<Graphics::StorageTexture> from;
    std::shared_ptr<Graphics::StorageTexture> to;
    const char* refusal = nullptr;
};

AliasPlan PlanAlias(const Graphics::Context& context, std::uint64_t destination, std::uint64_t source, std::size_t bytes) {
    AliasPlan plan;
    plan.from = Graphics::StorageTexture::FindLive(source, bytes);
    if (plan.from == nullptr) {
        plan.refusal = "no source";
        return plan;
    }
    plan.to = Graphics::StorageTexture::FindLive(destination, bytes);
    plan.refusal = Graphics::StorageTexture::CopyRefusal(context, plan.to.get(), *plan.from, destination);
    return plan;
}

bool AliasCopy(const Graphics::Context& context, std::uint64_t destination, std::uint64_t source, std::size_t bytes) {
    if (CopyAliasDisabled()) return false;
    auto plan = PlanAlias(context, destination, source, bytes);
    if (plan.from == nullptr) {
        copyAliasNoSource.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    if (plan.refusal != nullptr) {
        if (plan.to != nullptr && !plan.to->SameSurfaceShape(*plan.from)) {
            copyAliasShape.fetch_add(1, std::memory_order_relaxed);
        } else {
            static std::atomic<int> reported{0};
            if (reported.fetch_add(1) < 8) std::fprintf(stderr, "[copy] image copy 0x%llx -> 0x%llx (0x%zx bytes) refused: %s\n", static_cast<unsigned long long>(source), static_cast<unsigned long long>(destination), bytes, plan.refusal);
            copyAliasRefused.fetch_add(1, std::memory_order_relaxed);
        }
        return false;
    }
    if (plan.to == nullptr) {
        auto resource = plan.from->Descriptor();
        resource.baseAddress = destination;
        resource.dccAddress = 0;
        plan.to = Graphics::CachedStorageSurface(context, resource);
        copyAliasCreated.fetch_add(1, std::memory_order_relaxed);
    }
    const char* refusal = nullptr;
    if (!plan.to->CopyFrom(*plan.from, refusal)) throw std::runtime_error(std::string("AGC graphics: an image copy its check allowed was refused: ") + refusal);
    copiesAliased.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool AliasCopyPossible(const Graphics::Context& context, std::uint64_t destination, std::uint64_t source, std::size_t bytes) {
    return !CopyAliasDisabled() && PlanAlias(context, destination, source, bytes).refusal == nullptr;
}

}

VulkanDevice::CopyVerification VulkanDevice::CopyVerifyCounts() {
    return {copiesVerified.load(std::memory_order_relaxed), copySourceChanged.load(std::memory_order_relaxed), copyDestinationChanged.load(std::memory_order_relaxed), copyReaderMissed.load(std::memory_order_relaxed)};
}

namespace {
std::atomic<std::uint64_t> computePipelinesCreated{0};
std::atomic<std::uint64_t> computePipelinesCreatedLocked{0};
}

VulkanDevice::ComputePipelineCreation VulkanDevice::ComputePipelineCounts() {
    return {computePipelinesCreated.load(std::memory_order_relaxed), computePipelinesCreatedLocked.load(std::memory_order_relaxed)};
}

VulkanDevice::CopyAliasing VulkanDevice::CopyAliasCounts() {
    return {copiesAliased.load(std::memory_order_relaxed), copyAliasCreated.load(std::memory_order_relaxed), copyAliasNoSource.load(std::memory_order_relaxed), copyAliasShape.load(std::memory_order_relaxed), copyAliasRefused.load(std::memory_order_relaxed)};
}

bool VulkanDevice::completionStoresOver(std::uint64_t address, std::size_t bytes) const {
    static const bool alwaysSync = std::getenv("APS5_COPY_SYNC") != nullptr;
    if (alwaysSync || state->CopiedWriterOverlaps(address, bytes)) return true;
    return Graphics::Recorder::PendingCompletionLabels() != 0 && state->recorder->CompletionLabelIn(address, bytes);
}

bool VulkanDevice::copyWaitsOnCompletions(std::uint64_t address, std::size_t bytes) const {
    return state->recorder->PendingWriteOverlaps(address, bytes) && completionStoresOver(address, bytes);
}

bool VulkanDevice::GpuCopyPossible(std::uint64_t destination, std::uint64_t source, std::size_t bytes) const {
    if (!state->recorder || bytes == 0 || copyWaitsOnCompletions(source, bytes) || copyWaitsOnCompletions(destination, bytes)) return false;
    const auto context = graphicsContext();
    if (AliasCopyPossible(context, destination, source, bytes)) return true;
    return Graphics::HostImportFor(context, destination, bytes) != nullptr && Graphics::HostImportFor(context, source, bytes) != nullptr;
}

VulkanDevice::CopyOutcome VulkanDevice::CopyBuffer(std::uint64_t destination, std::uint64_t source, std::size_t bytes, std::size_t cpuMax, std::size_t gpuMax, std::size_t knownMax, std::uint64_t programAddress, std::uint32_t queue, const CopyWriterNote& noteWriter) {
    CopyOutcome outcome{2, false, 0, CopyOutcome::None, false, false, 0, 0, 0, 0, false};
    if (!state->recorder || bytes == 0) return outcome;
    auto& recorder = *state->recorder;
    // Finished batches were retired by the caller (Driver.cpp copyBuffer, on the graphics worker
    // only, as WriteLabelOnGpu reaps), so their writes neither refuse the CPU copy nor force a wait.
    static const bool waitForSource = std::getenv("APS5_COPY_WAIT_SOURCE") != nullptr;
    static const bool verify = std::getenv("APS5_COPY_VERIFY") != nullptr;
    // A store a batch's completion makes on the CPU over the range (a copied buffer's write-back,
    // a completion-deferred label): it lands only when that batch is reaped, whatever its fence.
    const auto completionStoreOverlaps = [&](std::uint64_t address) { return completionStoresOver(address, bytes); };
    // The CPU decision (see the header): pure queries, in the order that makes the first failing
    // test the reason; a signaled writer of the source is fine (its in-place stores are final in
    // host memory) as long as no completion store or label of it is still to land.
    bool settledBySignal = false;
    const auto refusal = [&]() -> int {
        settledBySignal = false;
        if (bytes > cpuMax) return CopyOutcome::Size;
        // Both lists: an image whose write-back is in progress on another thread (moved to flushing,
        // waiting for the GPU mutex) still stores over the range once this hold ends.
        const std::array<std::pair<std::uint64_t, std::uint64_t>, 2> ranges{{{source, source + bytes}, {destination, destination + bytes}}};
        if (Graphics::StorageTexture::AnyPendingOverlaps(ranges)) return CopyOutcome::Image;
        // Results retiled into a unit shadow are not in the import's bytes: the GPU path publishes them.
        if (Graphics::AnyShadowedOverlaps(ranges)) return CopyOutcome::Shadow;
        if (recorder.PendingWriteOverlaps(source, bytes)) {
            if (!recorder.PendingWriteSettled(source, bytes)) return CopyOutcome::SourcePending;
            if (completionStoreOverlaps(source) || recorder.PendingLabelIn(source, bytes)) return CopyOutcome::SourceUnsettled;
            settledBySignal = true;
        }
        if (recorder.PendingWriteOverlaps(destination, bytes)) {
            // As for the source: a signaled writer's in-place stores are final and the memcpy lands
            // after them; only a completion store of it still to run refuses (compute queues never
            // reap, so a finished GPU copy into the same record stays in flight for a while).
            // Debug aid: APS5_COPY_DESTINATION_STRICT=1 refuses any pending writer, as before.
            static const bool strict = std::getenv("APS5_COPY_DESTINATION_STRICT") != nullptr;
            if (strict || !recorder.PendingWriteSettled(destination, bytes)) return CopyOutcome::DestinationPending;
            if (completionStoreOverlaps(destination)) return CopyOutcome::DestinationUnsettled;
        }
        if (recorder.PendingLabelIn(destination, bytes)) return CopyOutcome::Label;
        if (Graphics::Recorder::ReadTracking() ? recorder.PendingReadOverlaps(destination, bytes) : !recorder.Idle()) return CopyOutcome::Reader;
        return CopyOutcome::None;
    };
    outcome.reason = refusal();
    if (outcome.reason == CopyOutcome::SourcePending && waitForSource && queue != 0) {
        // The experiment: only when the producer is the oldest batch in flight (the wait finishes
        // nothing else), never on the frame-critical queue 0, unlocked (this hold is the outermost).
        if (const auto info = recorder.DescribePendingWrite(source, bytes); info.has_value() && !info->open && info->batchesToFinish == 1) {
            const auto waitStart = std::chrono::steady_clock::now();
            Graphics::Recorder::CountSync(4);
            recorder.SyncThrough(source, bytes, true);
            outcome.waitedForSource = true;
            outcome.waitMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - waitStart).count();
            outcome.reason = refusal();
        }
    }
    if (outcome.reason == CopyOutcome::None) {
        // A plain CPU store, stamped for the write watch, with no pending write to note (nothing is
        // outstanding on the range) and nothing recorded.
        std::vector<std::byte> expected;
        if (verify) expected.assign(reinterpret_cast<const std::byte*>(source), reinterpret_cast<const std::byte*>(source) + bytes);
        GuestMemory::StoreOwnBytes(destination, bytes, [&] { std::memcpy(reinterpret_cast<void*>(destination), reinterpret_cast<const void*>(source), bytes); });
        outcome.path = 0;
        outcome.sourceSettledBySignal = settledBySignal;
        if (verify) {
            // The rule's reader answer without the fence shortcut: a reader still unsignaled now
            // would have been unsignaled at the decision a moment ago.
            if (const auto reader = recorder.DescribePendingRead(destination, bytes); reader.has_value() && !reader->signaled) copyReaderMissed.fetch_add(1, std::memory_order_relaxed);
            Graphics::Recorder::CountSync(4);
            recorder.Sync();
            copiesVerified.fetch_add(1, std::memory_order_relaxed);
            if (GuestMemory::Accessible(reinterpret_cast<const void*>(source), bytes) && std::memcmp(expected.data(), reinterpret_cast<const void*>(source), bytes) != 0) copySourceChanged.fetch_add(1, std::memory_order_relaxed);
            if (GuestMemory::Accessible(reinterpret_cast<const void*>(destination), bytes) && std::memcmp(expected.data(), reinterpret_cast<const void*>(destination), bytes) != 0) copyDestinationChanged.fetch_add(1, std::memory_order_relaxed);
        }
        return outcome;
    }
    if (outcome.reason == CopyOutcome::Reader) {
        if (const auto reader = recorder.DescribePendingRead(destination, bytes)) {
            outcome.readerSerial = reader->serial;
            outcome.readerQueue = reader->queue;
            outcome.readerKind = static_cast<int>(reader->kind);
            outcome.readerOpen = reader->open;
        }
    }
    const auto context = graphicsContext();
    // The ordering decision of FillBuffer, for both ranges: GPU stores recorded earlier are ordered
    // before the transfer (or the device copy below) by its barrier; a store a batch's completion
    // makes on the CPU must land before the copy reads the source or writes the destination, so
    // the copy waits for that batch (SyncThrough) first.
    const bool waitSource = copyWaitsOnCompletions(source, bytes);
    const bool waitDestination = copyWaitsOnCompletions(destination, bytes);
    if (waitSource || waitDestination) {
        outcome.synced = true;
        const auto syncStart = std::chrono::steady_clock::now();
        if (waitSource) {
            Graphics::Recorder::CountSync(4);
            recorder.SyncThrough(source, bytes);
        }
        if (waitDestination) {
            Graphics::Recorder::CountSync(4);
            recorder.SyncThrough(destination, bytes);
        }
        outcome.syncMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - syncStart).count();
    }
    if (AliasCopy(context, destination, source, bytes)) {
        noteWriter({}, 0);
        recorder.NotePendingWrite(destination, bytes);
        Graphics::StorageTexture::FlushPending(destination, bytes, nullptr, "buffer copy destination", Graphics::PublishScope::Whole);
        outcome.path = 3;
        return outcome;
    }
    if (bytes > gpuMax) {
        outcome.path = 8;
        return outcome;
    }
    // The GPU path. Results still on the GPU: the source's must be stored before the transfer reads
    // it, the destination's would be stored over the copy later (a flush into an import records
    // the retile and notes it as a pending write, which is why it comes after the CPU decision).
    Graphics::StorageTexture::FlushPending(source, bytes, nullptr, "buffer copy source", Graphics::PublishScope::Whole);
    Graphics::StorageTexture::FlushPending(destination, bytes, nullptr, "buffer copy destination", Graphics::PublishScope::PartialUnits);
    // Looked up after the sync (its completions' write-backs can refresh the import table and
    // retire an import) and the flushes (which may import memory themselves).
    const Graphics::HostImport* destinationImport = Graphics::HostImportFor(context, destination, bytes);
    const Graphics::HostImport* sourceImport = destinationImport != nullptr ? Graphics::HostImportFor(context, source, bytes) : nullptr;
    if (destinationImport == nullptr || sourceImport == nullptr) return outcome;
    using CommandClass = Graphics::Recorder::CommandClass;
    // A queued DCC key store over either range must land before the transfer reads or writes it.
    recorder.FlushKeyStoresOverlapping(source, bytes);
    recorder.FlushKeyStoresOverlapping(destination, bytes);
    // A queued label store in either range lands before the transfer reads or writes it.
    recorder.FlushStoresOverlapping(source, bytes);
    recorder.FlushStoresOverlapping(destination, bytes);
    VkAccessFlags covered = 0;
    const auto commands = recorder.Commands(&covered);
    // The class range covers the barriers too; the program-keyed range inside it is the transfer.
    const auto classTiming = recorder.BeginGpuTiming(CommandClass::Copy);
    if (Graphics::Recorder::BarrierValidate()) {
        const std::pair<std::uint64_t, std::uint64_t> read{source, source + bytes}, written{destination, destination + bytes};
        recorder.NoteAccess(CommandClass::Copy, Graphics::Recorder::Access{std::span(&read, 1), std::span(&written, 1), {}, VK_PIPELINE_STAGE_TRANSFER_BIT});
    }
    // Every earlier recorded read or write of either range precedes the transfer, whichever stage
    // made it, and host stores (the CPU copies above, the game's) are visible to it; the previous
    // command's trailing barrier already did that when it covered transfer reads and writes.
    constexpr VkAccessFlags transferAccess = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    if ((covered & transferAccess) == transferAccess && Graphics::Recorder::MergeBarriers()) {
        Graphics::Recorder::CountMerged(CommandClass::Copy);
    } else {
        Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, transferAccess);
        Graphics::Recorder::CountBarriers(CommandClass::Copy);
    }
    const auto gpuTiming = recorder.BeginGpuTiming(programAddress);
    const VkBufferCopy region{Graphics::HostImportOffset(*sourceImport, source), Graphics::HostImportOffset(*destinationImport, destination), bytes};
    context.Resolved(&Graphics::DeviceFunctions::cmdCopyBuffer, "vkCmdCopyBuffer")(commands, sourceImport->buffer, destinationImport->buffer, 1, &region);
    recorder.EndGpuTiming(gpuTiming, bytes);
    // The copied bytes are visible to everything recorded after and to the host, as after a fill.
    constexpr VkAccessFlags copiedAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_HOST_READ_BIT;
    Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, copiedAccess);
    Graphics::Recorder::CountBarriers(CommandClass::Copy);
    recorder.EndGpuTiming(classTiming, bytes);
    recorder.MarkCovered(copiedAccess);
    recorder.NotePendingRead(source, bytes, Graphics::Recorder::ReadKind::CopySource);
    GuestMemory::MarkWritten(destination, bytes);
    recorder.NotePendingWrite(destination, bytes);
    // The destination's bytes after the transfer are the source's now when nothing recorded,
    // pending or still to land can change the source before the transfer reads it (a CPU store
    // to it in between is a hardware race too): the caller's ring entry carries them, stamped
    // with the generation this store gets, and goes in ahead of the note.
    const auto generation = GuestMemory::MarkWritten(destination, bytes);
    const std::array<std::pair<std::uint64_t, std::uint64_t>, 1> sourceRange{{{source, source + bytes}}};
    const bool sourceCurrent = bytes <= knownMax && generation != 0 && !recorder.PendingWriteOverlaps(source, bytes) && !recorder.PendingLabelIn(source, bytes) && !completionStoreOverlaps(source) && !Graphics::StorageTexture::AnyPendingOverlaps(sourceRange) && !Graphics::AnyShadowedOverlaps(sourceRange);
    noteWriter(sourceCurrent ? std::span(reinterpret_cast<const std::byte*>(source), bytes) : std::span<const std::byte>{}, sourceCurrent ? generation : 0);
    recorder.NotePendingWrite(destination, bytes);
    outcome.path = 1;
    return outcome;
}

void* VulkanDevice::Window() const {
    return state->window;
}

void VulkanDevice::Resize(std::uint32_t width, std::uint32_t height) {
    APS5_LOG_OUT_DEBUG("Resize requested=%ux%u current=%ux%u", width, height, state->extent.width, state->extent.height);
    require(state->swapchain != VK_NULL_HANDLE, "cannot resize an unavailable swapchain");
    if (width == 0 || height == 0) {
        state->extent = {0, 0};
        return;
    }
    if (state->extent.width == width && state->extent.height == height) return;
    recreateSwapchain(width, height, false);
}

void VulkanDevice::recreateSwapchain(std::uint32_t width, std::uint32_t height, bool force) {
    VkSurfaceCapabilitiesKHR surface{};
    check(state->InstanceFunction<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>("vkGetPhysicalDeviceSurfaceCapabilitiesKHR")(state->physical, state->surface, &surface), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR resize");
    // The window's drawable size and the surface's extent can disagree for a moment while the window
    // is being resized; the swapchain must match the surface, and the next present catches up.
    if (surface.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        width = surface.currentExtent.width;
        height = surface.currentExtent.height;
        if (width == 0 || height == 0) {
            WaitIdle();
            state->extent = {0, 0};
            return;
        }
        if (!force && state->extent.width == width && state->extent.height == height) return;
    }
    WaitIdle();
    refreshSurfaceFormats();
    if (state->surfaceColorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT) {
        const bool offered = std::any_of(state->surfaceFormats.begin(), state->surfaceFormats.end(), [&](const VkSurfaceFormatKHR& format) { return format.format == state->surfaceFormat && format.colorSpace == state->surfaceColorSpace; });
        if (!offered || !AgcDriver::CurrentHostHdr().available) useSurfaceFormat({VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});
    }
    require(width >= surface.minImageExtent.width && width <= surface.maxImageExtent.width && height >= surface.minImageExtent.height && height <= surface.maxImageExtent.height, "unsupported resized output extent");
    require((surface.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0 && (surface.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) != 0 && (surface.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0, "resized surface capabilities are unsupported");
    VkSwapchainCreateInfoKHR create{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    create.surface = state->surface;
    create.minImageCount = surface.minImageCount;
    create.imageFormat = state->surfaceFormat;
    create.imageColorSpace = state->surfaceColorSpace;
    create.imageExtent = {width, height};
    create.imageArrayLayers = 1;
    create.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    create.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create.presentMode = state->presentMode;
    create.oldSwapchain = state->swapchain;
    state->retiredSwapchains.reserve(state->retiredSwapchains.size() + 1);
    VkSwapchainKHR replacement = VK_NULL_HANDLE;
    check(state->DeviceFunction<PFN_vkCreateSwapchainKHR>("vkCreateSwapchainKHR")(state->device, &create, nullptr, &replacement), "vkCreateSwapchainKHR resize");
    state->retiredSwapchains.push_back({state->swapchain, std::move(state->rendered)});
    state->swapchain = replacement;
    state->extent = create.imageExtent;
    auto getImages = state->DeviceFunction<PFN_vkGetSwapchainImagesKHR>("vkGetSwapchainImagesKHR");
    std::uint32_t count = 0;
    check(getImages(state->device, replacement, &count, nullptr), "vkGetSwapchainImagesKHR resize");
    state->images.resize(count);
    check(getImages(state->device, replacement, &count, state->images.data()), "vkGetSwapchainImagesKHR resize");
    state->images.resize(count);
    state->rendered.assign(count, VK_NULL_HANDLE);
    if (state->surfaceColorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT) setHdrMetadata();
    APS5_LOG_OUT_DEBUG("Resize complete swapchain=%p extent=%ux%u images=%u", reinterpret_cast<void*>(state->swapchain), state->extent.width, state->extent.height, count);
}

void VulkanDevice::refreshSurfaceFormats() {
    const auto getFormats = state->InstanceFunction<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>("vkGetPhysicalDeviceSurfaceFormatsKHR");
    const auto displayGeneration = AgcDriver::CurrentHostHdr().displayGeneration;
    std::uint32_t count = 0;
    check(getFormats(state->physical, state->surface, &count, nullptr), "vkGetPhysicalDeviceSurfaceFormatsKHR");
    std::vector<VkSurfaceFormatKHR> formats(count);
    check(getFormats(state->physical, state->surface, &count, formats.data()), "vkGetPhysicalDeviceSurfaceFormatsKHR");
    formats.resize(count);
    constexpr VkFormatFeatureFlags target = VK_FORMAT_FEATURE_BLIT_DST_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    std::erase_if(formats, [&](const VkSurfaceFormatKHR& format) {
        if (format.colorSpace != VK_COLOR_SPACE_HDR10_ST2084_EXT) return false;
        VkFormatProperties properties{};
        graphicsContext().formatProperties(state->physical, format.format, &properties);
        return (properties.optimalTilingFeatures & target) != target;
    });
    state->surfaceFormats = std::move(formats);
    state->surfaceDisplayGeneration = displayGeneration;
    constexpr VkFormatFeatureFlags scalerSource = VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    const std::array<VkFormat, 2> pqTexels{VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_A2R10G10B10_UNORM_PACK32};
    const bool sources = std::all_of(pqTexels.begin(), pqTexels.end(), [&](VkFormat format) {
        VkFormatProperties properties{};
        graphicsContext().formatProperties(state->physical, format, &properties);
        return (properties.optimalTilingFeatures & scalerSource) == scalerSource;
    });
    AgcDriverSetSurfaceHdr10_nid_postfix(state->swapchainColorSpaces && state->hdrMetadata && sources && Hdr10SurfaceFormat(state->surfaceFormats, VK_FORMAT_UNDEFINED).has_value());
}

void VulkanDevice::setHdrMetadata() {
    const auto host = AgcDriver::CurrentHostHdr();
    state->metadataGeneration = host.generation;
    require(host.available && state->hdrMetadata && state->surfaceColorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT && state->swapchain != VK_NULL_HANDLE, "HDR metadata needs HDR output, an HDR10 swapchain and VK_EXT_hdr_metadata");
    VkHdrMetadataEXT metadata{VK_STRUCTURE_TYPE_HDR_METADATA_EXT};
    metadata.displayPrimaryRed = {0.708f, 0.292f};
    metadata.displayPrimaryGreen = {0.170f, 0.797f};
    metadata.displayPrimaryBlue = {0.131f, 0.046f};
    metadata.whitePoint = {0.3127f, 0.3290f};
    metadata.maxLuminance = host.display.maxLuminance;
    metadata.minLuminance = host.display.minLuminance;
    metadata.maxContentLightLevel = PqToSdrWhiteNits(host);
    metadata.maxFrameAverageLightLevel = std::min(host.display.maxFullFrameLuminance, metadata.maxContentLightLevel);
    state->DeviceFunction<PFN_vkSetHdrMetadataEXT>("vkSetHdrMetadataEXT")(state->device, 1, &state->swapchain, &metadata);
    std::fprintf(stderr, "[videoout] HDR metadata: mastering %.1f to %.4f nits, MaxCLL %.1f nits, MaxFALL %.1f nits\n", metadata.maxLuminance, metadata.minLuminance, metadata.maxContentLightLevel, metadata.maxFrameAverageLightLevel);
}

void VulkanDevice::useSurfaceFormat(VkSurfaceFormatKHR format) {
    state->surfaceFormat = format.format;
    state->surfaceColorSpace = format.colorSpace;
    state->scaler = std::make_unique<PresentationScaler>(graphicsContext(), VK_FORMAT_B8G8R8A8_UNORM, state->surfaceFormat);
    state->hdrScaler.reset();
    state->hdrScalerSource = VK_FORMAT_UNDEFINED;
    const bool hdr = state->surfaceColorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT;
    SetPresentingHdr10(hdr);
    std::fprintf(stderr, "[videoout] presenting %s: swapchain format %d in color space %d\n", hdr ? "HDR10" : "SDR", static_cast<int>(state->surfaceFormat), static_cast<int>(state->surfaceColorSpace));
}

void VulkanDevice::SelectDynamicRange(const DisplayBuffer* buffer) {
    GuestMemory::AssertGpuLockHeld("VulkanDevice::SelectDynamicRange");
    require(state->swapchain != VK_NULL_HANDLE, "cannot select the dynamic range of an unavailable swapchain");
    if (AgcDriver::CurrentHostHdr().displayGeneration != state->surfaceDisplayGeneration) refreshSurfaceFormats();
    const auto host = AgcDriver::CurrentHostHdr();
    const bool metadataStale = state->surfaceColorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT && host.available && host.generation != state->metadataGeneration;
    if (buffer == nullptr || state->extent.width == 0 || state->extent.height == 0) {
        if (metadataStale) setHdrMetadata();
        return;
    }
    const auto wanted = DynamicRangeSurfaceFormat(state->surfaceFormats, buffer->pixelFormat, host.available);
    if (wanted.format == state->surfaceFormat && wanted.colorSpace == state->surfaceColorSpace) {
        if (metadataStale) setHdrMetadata();
        return;
    }
    WaitIdle();
    useSurfaceFormat(wanted);
    recreateSwapchain(state->extent.width, state->extent.height, true);
}

void VulkanDevice::ensureHdrScaler(VkFormat source) {
    if (state->hdrScaler != nullptr && state->hdrScalerSource == source) return;
    if (state->SharedSlotInFlight()) RetirePresents(0);
    state->hdrScaler = std::make_unique<PresentationScaler>(graphicsContext(), source, state->surfaceFormat);
    state->hdrScalerSource = source;
}

bool VulkanDevice::Presentable() const {
    return state->extent.width != 0 && state->extent.height != 0;
}

bool VulkanDevice::PresentClear(std::uint32_t width, std::uint32_t height, bool opaque) {
    APS5_LOG_OUT_DEBUG("PresentClear width=%u height=%u opaque=%u", width, height, static_cast<unsigned>(opaque));
    return present(width, height, opaque, {});
}

void VulkanDevice::PresentPixels(std::uint32_t width, std::uint32_t height, std::span<const std::byte> pixels) {
    APS5_LOG_OUT("PresentPixels width=%u height=%u pixels=%zu expected=%llu", width, height, pixels.size(), static_cast<unsigned long long>(width) * height * 4u);
    require(width != 0 && height != 0 && width <= 16384 && height <= 16384, "invalid display image extent");
    require(pixels.size() == static_cast<std::uint64_t>(width) * height * 4, "invalid display pixel buffer size");
    // Debug aid: APS5_DUMP_FRAMES=<n> saves the first n presented frames as frame_<index>.bmp.
    static const int dumpLimit = [] {
        const char* value = std::getenv("APS5_DUMP_FRAMES");
        return value ? std::atoi(value) : 0;
    }();
    static int dumped = 0;
    if (frameDumpOutput.Enabled_nid_no_patch() && dumped < dumpLimit) {
        char name[32];
        std::snprintf(name, sizeof(name), "frame_%03d.bmp", dumped++);
        frameDumpOutput.Write_nid_no_patch(name, width, height, pixels);
    }
    // Tests and tools present pixels synchronously; the game path splits the steps (see Driver::Present).
    if (!present(width, height, true, pixels)) return;
    FinishPresent();
    QueuePresent();
}

namespace {

// APS5_DUMP_SCALE=<n> (default 4) keeps every n-th pixel in each direction, so long runs of 4K
// frames stay small.
std::uint32_t DumpScale() {
    static const std::uint32_t scale = [] {
        const char* value = std::getenv("APS5_DUMP_SCALE");
        if (value == nullptr) return std::uint32_t{4};
        std::uint32_t parsed = 0;
        const auto end = value + std::strlen(value);
        const auto result = std::from_chars(value, end, parsed);
        require(result.ec == std::errc{} && result.ptr == end && parsed > 0 && parsed <= 16384, "APS5_DUMP_SCALE must be between 1 and 16384");
        return parsed;
    }();
    return scale;
}

// frame_<index>.bmp: every DumpScale()-th pixel of a BGRA8 image, 32-bit top-down.
void WriteFrameBmp(int index, std::uint32_t fullWidth, std::uint32_t fullHeight, std::span<const std::byte> full, std::uint32_t scale) {
    require(scale != 0 && full.size() == static_cast<std::size_t>(fullWidth) * fullHeight * 4, "invalid frame dump pixels");
    if (!frameDumpOutput.Enabled_nid_no_patch()) return;
    const std::uint32_t width = (fullWidth + scale - 1) / scale;
    const std::uint32_t height = (fullHeight + scale - 1) / scale;
    std::vector<std::byte> pixels(static_cast<std::size_t>(width) * height * 4);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) std::memcpy(pixels.data() + (static_cast<std::size_t>(y) * width + x) * 4, full.data() + (static_cast<std::size_t>(y) * scale * fullWidth + x * scale) * 4, 4);
    }
    char name[32];
    std::snprintf(name, sizeof(name), "frame_%03d.bmp", index);
    frameDumpOutput.Write_nid_no_patch(name, width, height, pixels);
}

ResidentPresent ResidentPresentable(const Graphics::Context& context, const Graphics::StorageTexture& image, const DisplayBuffer& buffer, VkFilter& filter, std::uint32_t& layer) {
    const auto& descriptor = image.Descriptor();
    if (descriptor.width != buffer.width || descriptor.height != buffer.height || descriptor.mipCount != 1 || image.ImageDepth() != 1) return ResidentPresent::None;
    const auto slice = ResidentPresentSlice(descriptor.baseAddress, image.GuestBytes(), image.ImageLayers(), buffer.address, DisplayBufferSize(buffer));
    if (!slice) return ResidentPresent::None;
    layer = *slice;
    const auto format = Graphics::StorageFormatForGuest(context, descriptor.format);
    VkFormatProperties properties{};
    context.formatProperties(context.physical, format, &properties);
    const bool blitSource = (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) != 0;
    filter = (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0 ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    return ResidentPresentPath(format, buffer.pixelFormat, blitSource);
}

std::shared_ptr<Graphics::StorageTexture> PresentableResident(const Graphics::Context& context, const DisplayBuffer& buffer, VkFilter& filter, bool& pending, bool& convert, std::uint32_t& layer) {
    auto resident = Graphics::StorageTexture::FindPendingSlice(buffer.address, DisplayBufferSize(buffer));
    pending = resident != nullptr;
    convert = false;
    layer = 0;
    if (resident == nullptr) return resident;
    const auto path = ResidentPresentable(context, *resident, buffer, filter, layer);
    if (path == ResidentPresent::None) resident.reset();
    convert = path == ResidentPresent::Convert;
    return resident;
}

Graphics::DccKeys ResidentKeys(const Graphics::StorageTexture& resident) {
    const auto& own = resident.Descriptor();
    GuestMemory::CollectWritesUncached(own.dccAddress, Graphics::DccKeyBytes(resident.GuestBytes()));
    return Graphics::ProvedClearKeys(own, resident.GuestBytes(), resident.KeyProof());
}

bool ResidentServesDisplay(const Graphics::StorageTexture& resident, const DisplayBuffer& buffer) {
    const auto keys = DisplayBufferKeys(buffer);
    if (keys == Graphics::DccKeys::ClearRegister) {
        char message[320];
        std::snprintf(message, sizeof(message), "VideoOut: display buffer 0x%llx reads register-clear DCC keys at 0x%llx over the pending image 0x%llx (DCC 0x%llx, filled keys %s): whether its results precede the clear is not modeled", static_cast<unsigned long long>(buffer.address), static_cast<unsigned long long>(buffer.dccAddress), static_cast<unsigned long long>(resident.Descriptor().baseAddress), static_cast<unsigned long long>(resident.Descriptor().dccAddress), Graphics::DccKeysName(resident.FilledKeys()));
        throw std::runtime_error(message);
    }
    if (Graphics::IsDccClear(resident.FilledKeys()) && resident.FilledKeys() == keys) return false;
    return Graphics::KeysServeSurface(resident.Descriptor().dccAddress, resident.UploadedKeys(), resident.FilledKeys(), buffer.dccAddress, [&] { return ResidentKeys(resident); }, [&] { return keys; });
}

bool ResidentKeysMoved(const Graphics::StorageTexture& resident) {
    return resident.Descriptor().dccAddress != 0 && ResidentKeys(resident) != resident.UploadedKeys();
}

VkClearColorValue PqClearValue(const DisplayBuffer& buffer) {
    const auto texel = DisplayBufferClearTexel(buffer, DisplayBufferKeys(buffer));
    std::uint32_t word = 0;
    std::memcpy(&word, texel.data(), sizeof(word));
    const auto color = PqClearColor(word, DisplayRedLow(buffer.pixelFormat));
    return VkClearColorValue{{color[0], color[1], color[2], color[3]}};
}

std::optional<std::array<std::byte, 4>> CompressedClearPixel(const DisplayBuffer& buffer, std::size_t bytes) {
    GuestMemory::FlushGpuWrites(buffer.address, bytes);
    const auto keys = DisplayBufferKeys(buffer);
    if (keys == Graphics::DccKeys::Uncompressed) return std::nullopt;
    return DisplayBufferClearPixel(buffer, keys);
}

std::vector<std::byte> DisplayPixels(const DisplayBuffer& buffer, const std::optional<std::array<std::byte, 4>>& cleared) {
    if (!cleared) return ReadDisplayBuffer(buffer);
    std::vector<std::byte> pixels(static_cast<std::size_t>(buffer.width) * buffer.height * cleared->size());
    for (std::size_t offset = 0; offset < pixels.size(); offset += cleared->size()) std::memcpy(pixels.data() + offset, cleared->data(), cleared->size());
    return pixels;
}

// Debug aid: APS5_NO_RESIDENT_PRESENT=1 always presents through guest memory.
bool NoResidentPresent() {
    static const bool no = std::getenv("APS5_NO_RESIDENT_PRESENT") != nullptr;
    return no;
}

// Debug aid: APS5_DUMP_FRAMES=<n> saves the first n presented display buffers as frame_<index>.bmp,
// read back by the blit's own submission and written after its fence (RetireSlot), or with
// APS5_NO_GPU_DUMP=1 decoded from the tiled guest buffer on the CPU, as before.
struct FrameDumps {
    int limit;
    bool cpu;
    int every = 1;
    int dumped = 0;
    std::uint64_t presents = 0;
};

FrameDumps& Dumps() {
    static FrameDumps dumps{[] {
        const char* value = std::getenv("APS5_DUMP_FRAMES");
        return value ? std::atoi(value) : 0;
    }(), std::getenv("APS5_NO_GPU_DUMP") != nullptr, [] {
        const char* value = std::getenv("APS5_DUMP_FRAMES_EVERY");
        return value ? std::max(std::atoi(value), 1) : 1;
    }()};
    return dumps;
}

}

bool VulkanDevice::PresentDisplayBuffer(const DisplayBuffer& buffer) {
    static const bool storeAtFlip = Graphics::StorageTexture::StoreAtFlipRequested(std::getenv("APS5_STORE_AT_FLIP"));
    if (storeAtFlip) Graphics::StorageTexture::FlushAllPending("flip");
    else Graphics::StorageTexture::FlushDumpSelected("flip");
    auto& dumps = Dumps();
    const bool passthrough = state->surfaceColorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT;
    require(!passthrough || DisplayPq(buffer.pixelFormat), "only a PQ display buffer is presented to the HDR10 swapchain");
    if (buffer.tilingMode == 1) {
        if (passthrough) ensureHdrScaler(DisplayTexelFormat(buffer.pixelFormat));
        const auto pixels = passthrough ? ReadDisplayBufferTexels(buffer) : ReadDisplayBuffer(buffer);
        bool dumpFrame = false;
        if (frameDumpOutput.Enabled_nid_no_patch() && dumps.dumped < dumps.limit && ++dumps.presents % static_cast<std::uint64_t>(dumps.every) == 0) {
            if (passthrough) {
                WriteFrameBmp(dumps.dumped++, buffer.width, buffer.height, ReadDisplayBuffer(buffer), DumpScale());
            } else if (dumps.cpu) {
                WriteFrameBmp(dumps.dumped++, buffer.width, buffer.height, pixels, DumpScale());
            } else {
                state->nextDumpIndex = dumps.dumped++;
                dumpFrame = true;
            }
        }
        const bool screenshot = takeScreenshot(dumpFrame);
        auto shot = screenshot && passthrough ? ReadDisplayBuffer(buffer) : std::vector<std::byte>{};
        if (!present(buffer.width, buffer.height, true, pixels, nullptr, nullptr, VK_FILTER_LINEAR, dumpFrame || (screenshot && !passthrough), false, nullptr, 0, passthrough)) {
            if (dumpFrame) --dumps.dumped;
            if (screenshot) Screenshot::Request();
            return false;
        }
        if (!shot.empty()) state->dumpWriter.Enqueue(0, buffer.width, buffer.height, 1, true, std::move(shot));
        return true;
    }
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    // The display buffer is usually a resident render target whose results are still on the GPU: it
    // is blitted from that image, which skips the write-back retile, the 33 MB read of guest memory,
    // the upload and the detile dispatch. The deferred write-back stays pending as for any storage
    // image.
    static std::uint64_t residentPresents = 0, refreshedPresents = 0, notPending = 0, unsuitable = 0, gpuDumps = 0;
    static auto lastReport = std::chrono::steady_clock::now();
    const auto bytes = DisplayBufferSize(buffer);
    std::shared_ptr<Graphics::StorageTexture> resident;
    VkFilter filter = VK_FILTER_LINEAR;
    bool convert = false;
    std::uint32_t layer = 0;
    if (!NoResidentPresent()) {
        bool pending = false;
        resident = PresentableResident(graphicsContext(), buffer, filter, pending, convert, layer);
        if (resident != nullptr && buffer.dccAddress != 0 && !ResidentServesDisplay(*resident, buffer)) {
            resident.reset();
            convert = false;
        }
        if (!pending) {
            ++notPending;
        } else if (resident == nullptr) {
            ++unsuitable;
        } else {
            // 64 KiB blocks the CPU wrote since the image last matched guest memory would be shown
            // stale (the write-back keeps the CPU's bytes for them): Refresh merges them into the
            // image first, as a sampled texture of the image would (see cachedTexture). The uncached
            // walk: the collect memo is per worker packet, so on this thread a memoized answer could
            // miss a CPU write that landed after a worker's walk of the same range.
            static const bool noAdopt = std::getenv("APS5_NO_PRESENT_EPOCH") != nullptr;
            if (!noAdopt) GuestMemory::AdoptLatestCollectEpoch();
            GuestMemory::CollectWritesUncached(buffer.address, bytes);
            if (!GuestMemory::UnchangedSince(buffer.address, bytes, resident->GenerationFor(buffer.address, bytes)) || (buffer.dccAddress != 0 && ResidentKeysMoved(*resident))) {
                if (resident->ImageLayers() > 1) resident->RefreshLayers(layer, 1);
                else resident->Refresh();
                ++refreshedPresents;
            }
            ++residentPresents;
        }
    }
    const auto cleared = buffer.dccAddress != 0 && resident == nullptr ? CompressedClearPixel(buffer, bytes) : std::nullopt;
    bool dumpFrame = false;
    if (frameDumpOutput.Enabled_nid_no_patch() && dumps.dumped < dumps.limit && ++dumps.presents % static_cast<std::uint64_t>(dumps.every) == 0) {
        if (dumps.cpu || (passthrough && resident == nullptr)) {
            WriteFrameBmp(dumps.dumped++, buffer.width, buffer.height, DisplayPixels(buffer, cleared), DumpScale());
        } else {
            state->nextDumpIndex = dumps.dumped++;
            dumpFrame = true;
            ++gpuDumps;
        }
    }
    if (profile && std::chrono::steady_clock::now() - lastReport > std::chrono::seconds(10)) {
        lastReport = std::chrono::steady_clock::now();
        AgcDriver::ProfilePrint_nid_no_patch("[flip] (cumulative): %llu presents from the resident image (%llu refreshed first), through guest memory: %llu not pending, %llu unsuitable; %llu GPU frame dumps\n", static_cast<unsigned long long>(residentPresents), static_cast<unsigned long long>(refreshedPresents), static_cast<unsigned long long>(notPending), static_cast<unsigned long long>(unsuitable), static_cast<unsigned long long>(gpuDumps));
    }
    const bool screenshot = takeScreenshot(dumpFrame);
    const bool cpuScreenshot = screenshot && passthrough && resident == nullptr;
    auto shot = cpuScreenshot ? DisplayPixels(buffer, cleared) : std::vector<std::byte>{};
    CaptureTrace::Log("present dump=%d address=%llx width=%u height=%u resident=%d generation=%llu", dumpFrame ? state->nextDumpIndex : -1, static_cast<unsigned long long>(buffer.address), buffer.width, buffer.height, resident != nullptr, static_cast<unsigned long long>(resident ? resident->Generation() : 0));
    if (passthrough && resident == nullptr) ensureHdrScaler(DisplayTexelFormat(buffer.pixelFormat));
    VkClearColorValue uniform{};
    if (cleared && passthrough) {
        uniform = PqClearValue(buffer);
    } else if (cleared) {
        const auto channel = [&](std::size_t index) { return static_cast<float>(std::to_integer<unsigned>((*cleared)[index])) / 255.0f; };
        uniform = {{channel(2), channel(1), channel(0), channel(3)}};
    }
    if (!present(buffer.width, buffer.height, true, {}, cleared ? nullptr : &buffer, resident, filter, dumpFrame || (screenshot && !cpuScreenshot), convert, cleared ? &uniform : nullptr, layer, passthrough)) {
        // A dropped frame (swapchain out of date) keeps the dump numbering contiguous.
        if (dumpFrame) --dumps.dumped;
        if (screenshot) Screenshot::Request();
        return false;
    }
    if (cpuScreenshot) state->dumpWriter.Enqueue(0, buffer.width, buffer.height, 1, true, std::move(shot));
    return true;
}

bool VulkanDevice::takeScreenshot(bool dumpFrame) {
    state->nextDumpScreenshot = !dumpFrame && state->dumpWriter.Idle() && Screenshot::Take();
    return state->nextDumpScreenshot;
}

bool VulkanDevice::AcquireImage() {
    PerformanceTimer timing("Vulkan.Acquire");
    require(state->swapchain != VK_NULL_HANDLE, "device has no swapchain");
    require(state->extent.width != 0 && state->extent.height != 0, "output window is minimized");
    require(!state->imageAcquired, "the previously acquired image was not presented");
    auto wait = state->DeviceFunction<PFN_vkWaitForFences>("vkWaitForFences");
    auto reset = state->DeviceFunction<PFN_vkResetFences>("vkResetFences");
    check(reset(state->device, 1, &state->acquireFence), "vkResetFences");
    APS5_LOG_CHARS_OUT_DEBUG("acquire fence reset");
    std::uint32_t index = 0;
    timing.Mark("fence_reset");
    // An out-of-date swapchain (the window changed) drops this frame; the next Resize recreates it.
    const auto acquired = state->DeviceFunction<PFN_vkAcquireNextImageKHR>("vkAcquireNextImageKHR")(state->device, state->swapchain, 0, VK_NULL_HANDLE, state->acquireFence, &index);
    if (acquired == VK_ERROR_OUT_OF_DATE_KHR) {
        state->extent = {0, 0};
        return false;
    }
    if (acquired == VK_NOT_READY || acquired == VK_TIMEOUT) {
        ++presentCounters.acquireDrops;
        return false;
    }
    if (acquired != VK_SUBOPTIMAL_KHR) check(acquired, "vkAcquireNextImageKHR");
    timing.Mark("acquire_image");
    APS5_LOG_OUT_DEBUG("vkAcquireNextImageKHR index=%u imageCount=%zu", index, state->images.size());
    check(wait(state->device, 1, &state->acquireFence, VK_TRUE, std::numeric_limits<std::uint64_t>::max()), "vkWaitForFences acquire");
    timing.Mark("acquire_fence_wait");
    APS5_LOG_OUT_DEBUG("Acquire fence complete index=%u", index);
    require(index < state->images.size() && index < state->rendered.size(), "acquired image index is out of range");
    auto& rendered = state->rendered[index];
    if (rendered != VK_NULL_HANDLE) {
        state->DestroyRetiredSwapchains();
    } else {
        VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        check(state->DeviceFunction<PFN_vkCreateSemaphore>("vkCreateSemaphore")(state->device, &semaphore, nullptr, &rendered), "vkCreateSemaphore presentation");
    }
    timing.Mark("retired_swapchains");
    state->acquiredIndex = index;
    state->imageAcquired = true;
    return true;
}

bool VulkanDevice::present(std::uint32_t width, std::uint32_t height, bool opaque, std::span<const std::byte> pixels, const DisplayBuffer* display, const std::shared_ptr<Graphics::StorageTexture>& resident, VkFilter residentFilter, bool dumpFrame, bool residentConvert, const VkClearColorValue* uniform, std::uint32_t residentLayer, bool passthrough) {
    PerformanceTimer timing("Vulkan.Present");
    APS5_LOG_OUT_DEBUG("present begin width=%u height=%u opaque=%u pixels=%zu", width, height, static_cast<unsigned>(opaque), pixels.size());
    require(state->swapchain != VK_NULL_HANDLE, "device has no swapchain");
    require(state->extent.width != 0 && state->extent.height != 0, "output window is minimized");
    require(!state->queuePending, "the previous presentation was not queued");
    require(!state->presentSlots.empty(), "device has no presentation slots");
    // The game path acquired the image before taking GpuMutex (Driver::Present); tests, tools and
    // APS5_SYNC_FLIP=1 acquire here.
    if (!state->imageAcquired && !AcquireImage()) return false;
    state->imageAcquired = false;
    const auto index = state->acquiredIndex;
    require(index < state->images.size() && index < state->rendered.size(), "acquired image index is out of range");
    auto& rendered = state->rendered[index];
    const bool clearOnly = pixels.empty() && display == nullptr && uniform == nullptr;
    require(!residentConvert || (resident != nullptr && display != nullptr), "a converted resident presentation needs its image and display buffer");
    require(uniform == nullptr || (pixels.empty() && display == nullptr && resident == nullptr), "a uniform presentation has no other source");
    const bool direct = resident != nullptr && !residentConvert;
    const bool drawn = resident != nullptr && residentConvert && !DisplayHalfFloat(display->pixelFormat);
    require(passthrough == (state->surfaceColorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT) || clearOnly, "a presentation's dynamic range differs from its swapchain's");
    require(!passthrough || drawn || clearOnly || (state->hdrScaler != nullptr && !dumpFrame), "an HDR10 presentation through the scaler needs the HDR10 scaler and dumps on the CPU");
    auto* scaler = passthrough && !drawn ? state->hdrScaler.get() : state->scaler.get();
    const float whiteNits = PqToSdrWhiteNits(AgcDriver::CurrentHostHdr());
    // The scaler's source image, the color transfer's staging and the upload buffer are single
    // objects an in-flight blit through them may still read: the paths using or re-creating them
    // wait for every slot first. The game path did so before taking the mutex
    // (PresentWaitsForSlots); this is the fallback.
    const bool shared = !clearOnly && !direct && !drawn;
    if (shared && state->SharedSlotInFlight()) RetirePresents(0);
    // The next slot is the oldest; a caller that did not retire it first (PresentPixels) waits here.
    if (state->presentSlots[state->presentCursor].inFlight) RetirePresents(state->presentSlots.size() - 1);
    auto& slot = state->presentSlots[state->presentCursor];
    require(!slot.inFlight, "presentation slot is still in flight");
    const bool overlay = PerformanceOverlay::Visible();
    if (display != nullptr && resident == nullptr) {
        static_cast<void>(DisplayBufferSize(*display));
        state->colorTransfer->Upload(display->address, width, height, Graphics::ColorTileMode::RenderTarget, DisplayTexelBytes(display->pixelFormat));
    }
    if (!pixels.empty()) state->Upload(pixels);
    // The frame's recorded work (and a refresh of the resident image) must reach the queue before the
    // presentation's own submission, which reads the image in queue order. No reap first: queue 0
    // reaps at its next packet, so the presenter's hold carries no completions.
    SubmitRecorded(false);
    std::chrono::steady_clock::time_point lastSubmittedAt{};
    const std::uint64_t batchesAtBlit = state->recorder ? state->recorder->NewestSubmitted(&lastSubmittedAt) : 0;
    CaptureTrace::Log("blit dump=%d batch=%llu slot=%zu image=%u direct=%d", dumpFrame ? state->nextDumpIndex : -1, static_cast<unsigned long long>(batchesAtBlit), state->presentCursor, index, direct);
    timing.Mark("pixel_upload");
    APS5_LOG_OUT_DEBUG("present source=%s bytes=%zu", pixels.empty() ? "clear" : "pixels", pixels.size());
    auto commands = slot.commands;
    check(state->DeviceFunction<PFN_vkResetCommandBuffer>("vkResetCommandBuffer")(commands, 0), "vkResetCommandBuffer");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check(state->DeviceFunction<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(commands, &begin), "vkBeginCommandBuffer");
    APS5_LOG_OUT_DEBUG("Presentation command buffer begin commands=%p image=%p", reinterpret_cast<void*>(commands), reinterpret_cast<void*>(state->images[index]));
    using CommandClass = Graphics::Recorder::CommandClass;
    Graphics::Recorder::CountPresent();
    if (Graphics::Recorder::GpuTimingEnabled()) {
        if (slot.queries == VK_NULL_HANDLE) {
            VkQueryPoolCreateInfo info{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
            info.queryType = VK_QUERY_TYPE_TIMESTAMP;
            info.queryCount = 2;
            if (state->DeviceFunction<PFN_vkCreateQueryPool>("vkCreateQueryPool")(state->device, &info, nullptr, &slot.queries) != VK_SUCCESS) slot.queries = VK_NULL_HANDLE;
        }
        if (slot.queries != VK_NULL_HANDLE) {
            state->DeviceFunction<PFN_vkCmdResetQueryPool>("vkCmdResetQueryPool")(commands, slot.queries, 0, 2);
            state->DeviceFunction<PFN_vkCmdWriteTimestamp>("vkCmdWriteTimestamp")(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, slot.queries, 0);
        }
        slot.blitBytes = static_cast<std::uint64_t>(state->extent.width) * state->extent.height * 4;
    }
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = state->images[index];
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    auto pipelineBarrier = state->DeviceFunction<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
    if (!drawn) {
        pipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        Graphics::Recorder::CountBarriers(CommandClass::PresentBlit);
    }
    if (clearOnly) {
        APS5_LOG_OUT_DEBUG("Recording swapchain clear opaque=%u image=%p", static_cast<unsigned>(opaque), reinterpret_cast<void*>(barrier.image));
        VkClearColorValue clear{};
        clear.float32[3] = opaque ? 1.0f : 0.0f;
        state->DeviceFunction<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, barrier.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &barrier.subresourceRange);
    } else {
        APS5_LOG_OUT_DEBUG("Recording swapchain scaled blit width=%u height=%u bytes=%zu buffer=%p image=%p", width, height, pixels.size(), reinterpret_cast<void*>(state->uploadBuffer), reinterpret_cast<void*>(barrier.image));
        require(scaler != nullptr && state->presentation != nullptr, "presentation scaler is unavailable");
        if (shared) scaler->EnsureSourceImage(width, height);
        const auto slotIndex = static_cast<std::size_t>(&slot - state->presentSlots.data());
        VkImageMemoryBarrier residentBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        const VkPipelineStageFlags residentStage = drawn ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT;
        const VkAccessFlags residentRead = drawn ? VK_ACCESS_SHADER_READ_BIT : VK_ACCESS_TRANSFER_READ_BIT;
        if (resident != nullptr) {
            // The draws and dispatches that produced the image were submitted before this command
            // buffer; their writes become visible to the blit here. The layout stays GENERAL, the
            // one the storage image is tracked in.
            residentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            residentBarrier.dstAccessMask = residentRead;
            residentBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            residentBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            residentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            residentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            residentBarrier.image = resident->Image();
            residentBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS};
            pipelineBarrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, residentStage, 0, 0, nullptr, 0, nullptr, 1, &residentBarrier);
            Graphics::Recorder::CountBarriers(CommandClass::PresentBlit);
            if (residentConvert && !drawn) {
                state->colorTransfer->DetileImage(commands, resident->Image(), VK_IMAGE_LAYOUT_GENERAL, width, height, Graphics::ColorTileMode::RenderTarget, DisplayRedLow(display->pixelFormat), DisplayTenBit(display->pixelFormat), DisplayHalfFloat(display->pixelFormat), residentLayer);
                scaler->RecordUpload(commands, state->colorTransfer->LinearBuffer());
            }
        } else {
            if (display != nullptr && passthrough) {
                state->colorTransfer->Detile(commands);
            } else if (display != nullptr) {
                const bool pq = DisplayPq(display->pixelFormat);
                state->colorTransfer->Detile(commands, DisplayRedLow(display->pixelFormat), DisplayTenBit(display->pixelFormat) && !pq, DisplayHalfFloat(display->pixelFormat), pq ? std::optional<float>(whiteNits) : std::nullopt);
            }
            if (uniform != nullptr) scaler->RecordClear(commands, *uniform);
            else scaler->RecordUpload(commands, display != nullptr ? state->colorTransfer->LinearBuffer() : state->uploadBuffer);
        }
        if (drawn) {
            state->presentation->Bind(slotIndex, resident->Image(), width, height, display->pixelFormat, whiteNits, residentLayer);
            state->presentation->RecordPresent(commands, slotIndex, barrier.image, state->surfaceFormat, state->surfaceColorSpace, state->extent, overlay ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        } else {
            scaler->RecordLetterbox(commands, width, height, barrier.image, state->extent.width, state->extent.height);
            if (direct) PresentationScaler::RecordBlitFrom(graphicsContext(), commands, resident->Image(), VK_IMAGE_LAYOUT_GENERAL, width, height, residentFilter, barrier.image, state->extent.width, state->extent.height, residentLayer);
            else scaler->RecordBlit(commands, barrier.image, state->extent.width, state->extent.height);
        }
        if (dumpFrame) {
            const auto scale = state->nextDumpScreenshot ? 1 : DumpScale();
            if (drawn || direct) {
                const auto extent = PresentationPass::DumpExtent(width, height, scale);
                slot.dumpWidth = extent.width;
                slot.dumpHeight = extent.height;
                slot.dumpScale = 1;
            } else {
                slot.dumpWidth = scaler->SourceWidth();
                slot.dumpHeight = scaler->SourceHeight();
                slot.dumpScale = scale;
            }
            const auto dumpBytes = static_cast<std::size_t>(slot.dumpWidth) * slot.dumpHeight * 4;
            if (!slot.dumpBuffer || slot.dumpBuffer->Bytes().size() != dumpBytes) slot.dumpBuffer = std::make_unique<Graphics::Buffer>(graphicsContext(), dumpBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            if (drawn) {
                state->presentation->RecordDump(commands, slotIndex, scale, slot.dumpBuffer->Handle());
            } else if (direct) {
                if (!slot.dumpScaler) slot.dumpScaler = std::make_unique<PresentationScaler>(graphicsContext(), VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM);
                slot.dumpScaler->EnsureSourceImage(slot.dumpWidth, slot.dumpHeight);
                slot.dumpScaler->RecordBlitInto(commands, resident->Image(), VK_IMAGE_LAYOUT_GENERAL, VK_FILTER_NEAREST, width, height, residentLayer);
                slot.dumpScaler->RecordReadback(commands, slot.dumpBuffer->Handle());
            } else {
                scaler->RecordReadback(commands, slot.dumpBuffer->Handle());
            }
        }
        if (resident != nullptr) {
            residentBarrier.srcAccessMask = residentRead;
            residentBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            pipelineBarrier(commands, residentStage | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &residentBarrier);
            Graphics::Recorder::CountBarriers(CommandClass::PresentBlit);
        }
    }
    if (overlay) {
        if (!state->overlay) state->overlay = std::make_unique<PerformanceOverlay>(graphicsContext(), state->presentSlots.size());
        state->overlay->NotePresent(std::chrono::steady_clock::now(), clearOnly ? VkExtent2D{} : VkExtent2D{width, height});
        state->overlay->Record(commands, static_cast<std::size_t>(&slot - state->presentSlots.data()), barrier.image, state->surfaceFormat, state->surfaceColorSpace, state->extent, drawn ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    } else if (!drawn) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = 0;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        pipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        Graphics::Recorder::CountBarriers(CommandClass::PresentBlit);
    }
    if (slot.queries != VK_NULL_HANDLE) state->DeviceFunction<PFN_vkCmdWriteTimestamp>("vkCmdWriteTimestamp")(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, slot.queries, 1);
    check(state->DeviceFunction<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(commands), "vkEndCommandBuffer");
    APS5_LOG_CHARS_OUT_DEBUG("Presentation command buffer recorded");
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &commands;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &rendered;
    timing.Mark("command_record_scale");
    // The slot's fence signaled (retired above) or was never used: the reset cannot block.
    check(state->DeviceFunction<PFN_vkResetFences>("vkResetFences")(state->device, 1, &slot.fence), "vkResetFences present");
    check(state->DeviceFunction<PFN_vkQueueSubmit>("vkQueueSubmit")(state->queue, 1, &submit, slot.fence), "vkQueueSubmit clear");
    const auto submittedAt = std::chrono::steady_clock::now();
    timing.Mark("queue_submit");
    APS5_LOG_CHARS_OUT_DEBUG("Presentation vkQueueSubmit OK");
    {
        std::lock_guard lock(state->presentMutex);
        // The image stays alive until the slot is retired (storage-cache eviction only drops the
        // cache's reference).
        slot.kept = resident;
        slot.imageIndex = index;
        slot.inFlight = true;
        slot.shared = shared;
        // Only a submitted readback is written after the fence (a failed submit tears the device down).
        slot.dumpRecorded = dumpFrame;
        slot.dumpScreenshot = dumpFrame && state->nextDumpScreenshot;
        slot.dumpIndex = state->nextDumpIndex;
        slot.recorderSerial = batchesAtBlit;
        slot.previousSerial = state->lastPresentSerial;
        slot.submittedAt = submittedAt;
        state->lastPresentSerial = batchesAtBlit;
        state->presentCursor = (state->presentCursor + 1) % state->presentSlots.size();
        state->queuePending = true;
        state->queueIndex = index;
        ++presentCounters.presents;
        if (auto* frame = PerformanceContext::Current()) {
            frame->NoteBlit(batchesAtBlit, lastSubmittedAt, submittedAt);
            // The serials are cumulative: only their difference (the batches other queues put
            // ahead of the blit since the flip packet) is summed; a record without a flip sample
            // (the drain paths) contributes nothing.
            const auto atFlip = frame->BatchesAtFlip();
            if (atFlip != 0 && batchesAtBlit >= atFlip) presentCounters.batchesAheadOfBlit += batchesAtBlit - atFlip;
            if (frame->FlipReached() != std::chrono::steady_clock::time_point{}) {
                presentCounters.blitSubmitAfterFlipMs += std::chrono::duration<double, std::milli>(submittedAt - frame->FlipReached()).count();
                presentCounters.lastSubmitAfterFlipMs += std::chrono::duration<double, std::milli>(std::max(lastSubmittedAt, submittedAt) - frame->FlipReached()).count();
            }
        }
    }
    return true;
}

std::size_t VulkanDevice::FlipInFlight() {
    static const std::size_t count = [] {
        if (std::getenv("APS5_SYNC_FLIP") != nullptr) return std::size_t{0};
        const char* value = std::getenv("APS5_FLIP_INFLIGHT");
        if (value == nullptr) return std::size_t{1};
        const long parsed = std::strtol(value, nullptr, 10);
        if (parsed >= 0 && parsed <= 2) return static_cast<std::size_t>(parsed);
        std::fprintf(stderr, "[present] APS5_FLIP_INFLIGHT=%s refused (0, 1 or 2 presentations may trail on the GPU); using 1\n", value);
        return std::size_t{1};
    }();
    return count;
}

VulkanDevice::PresentStatistics VulkanDevice::PresentCounts() {
    return presentCounters;
}

bool VulkanDevice::PresentWaitsForSlots(const DisplayBuffer* buffer) const {
    if (buffer == nullptr || !state->SharedSlotInFlight()) return false;
    if (buffer->tilingMode == 1) return true;
    if (NoResidentPresent()) return true;
    VkFilter filter = VK_FILTER_LINEAR;
    bool pending = false;
    bool convert = false;
    std::uint32_t layer = 0;
    return PresentableResident(graphicsContext(), *buffer, filter, pending, convert, layer) == nullptr;
}

void VulkanDevice::FlipBatches(std::uint64_t& submissions, std::uint64_t& unsignaled) const {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    submissions = state->recorder ? state->recorder->Submissions() : 0;
    unsignaled = profile && state->recorder ? state->recorder->UnsignaledBatches() : 0;
}

double VulkanDevice::RetirePresents(std::size_t keepInFlight) {
    auto& slots = state->presentSlots;
    if (slots.empty()) return 0;
    PerformanceTimer timing("Vulkan.Retire");
    std::lock_guard lock(state->presentMutex);
    const auto status = state->DeviceFunction<PFN_vkGetFenceStatus>("vkGetFenceStatus");
    auto inFlight = static_cast<std::size_t>(std::count_if(slots.begin(), slots.end(), [](const auto& slot) { return slot.inFlight; }));
    double waitedMs = 0;
    bool retired = false;
    // Oldest first from the cursor; the fences of one queue signal in submission order, so the
    // first unsignaled one ends the scan, and the ones beyond the bound are the oldest.
    for (std::size_t i = 0; i < slots.size() && inFlight != 0; ++i) {
        auto& slot = slots[(state->presentCursor + i) % slots.size()];
        if (!slot.inFlight) continue;
        if (inFlight <= keepInFlight) {
            const auto result = status(state->device, slot.fence);
            if (result == VK_NOT_READY) break;
            check(result, "vkGetFenceStatus present");
            timing.Mark("poll");
        } else {
            const auto start = std::chrono::steady_clock::now();
            state->WaitPresentFence(slot.fence);
            waitedMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            timing.Mark("wait");
        }
        const bool dump = slot.dumpRecorded;
        state->RetireSlot(slot);
        retired = true;
        if (dump) timing.Mark("dump");
        --inFlight;
    }
    if (retired) Graphics::CheckPixelFunctionLds(graphicsContext());
    return waitedMs;
}

double VulkanDevice::FinishPresent() {
    return RetirePresents(0);
}

void VulkanDevice::QueuePresent() {
    if (!state->queuePending) return;
    PerformanceTimer timing("Vulkan.QueuePresent");
    state->queuePending = false;
    const auto index = state->queueIndex;
    require(index < state->rendered.size(), "presented image index is out of range");
    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &state->rendered[index];
    present.swapchainCount = 1;
    present.pSwapchains = &state->swapchain;
    present.pImageIndices = &index;
    const auto presented = state->DeviceFunction<PFN_vkQueuePresentKHR>("vkQueuePresentKHR")(state->queue, &present);
    if (presented == VK_ERROR_OUT_OF_DATE_KHR || presented == VK_SUBOPTIMAL_KHR) state->extent = {0, 0};
    else check(presented, "vkQueuePresentKHR");
    timing.Mark("queue_present");
    APS5_LOG_OUT_DEBUG("vkQueuePresentKHR queued imageIndex=%u", index);
}

ShaderRecompiler::SpirvTarget VulkanDevice::ComputeTarget(std::uint32_t waveSize) const {
    auto target = Target();
    if (waveSize == 32u && state->computeWave32) target.subgroupSize = 32u;
    return target;
}

std::string VulkanDevice::DeviceName() const {
    return state->properties.deviceName;
}

std::string VulkanDevice::DriverInfo() const {
    return state->driverInfo.empty() ? std::to_string(state->properties.driverVersion) : state->driverInfo;
}

bool VulkanDevice::CalibrateClock(double& gpuNs, std::chrono::steady_clock::time_point& cpu) const {
    if (state->calibratedTimestamps == nullptr) return false;
    const std::array<VkCalibratedTimestampInfoKHR, 2> domains{VkCalibratedTimestampInfoKHR{VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_KHR, nullptr, VK_TIME_DOMAIN_DEVICE_KHR}, VkCalibratedTimestampInfoKHR{VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_KHR, nullptr, state->hostTimeDomain}};
    std::array<std::uint64_t, 2> stamps{};
    std::uint64_t deviation = 0;
    if (state->calibratedTimestamps(state->device, static_cast<std::uint32_t>(domains.size()), domains.data(), stamps.data(), &deviation) != VK_SUCCESS) return false;
#ifdef _WIN32
    LARGE_INTEGER counter{};
    LARGE_INTEGER frequency{};
    QueryPerformanceCounter(&counter);
    const auto now = std::chrono::steady_clock::now();
    QueryPerformanceFrequency(&frequency);
    const double sinceNs = static_cast<double>(static_cast<std::uint64_t>(counter.QuadPart) - stamps[1]) * 1e9 / static_cast<double>(frequency.QuadPart);
#else
    timespec monotonic{};
    clock_gettime(CLOCK_MONOTONIC, &monotonic);
    const auto now = std::chrono::steady_clock::now();
    const double sinceNs = static_cast<double>(static_cast<std::uint64_t>(monotonic.tv_sec) * 1000000000ull + static_cast<std::uint64_t>(monotonic.tv_nsec) - stamps[1]);
#endif
    gpuNs = static_cast<double>(stamps[0]) * state->properties.limits.timestampPeriod;
    cpu = now - std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double, std::nano>(sinceNs));
    return true;
}

const std::uint32_t* VulkanDevice::WorkGroupCountLimit() const {
    return state->properties.limits.maxComputeWorkGroupCount;
}

std::uint32_t VulkanDevice::VendorId() const {
    return state->properties.vendorID;
}

bool VulkanDevice::SupportsImageFormat(VkFormat format, VkImageUsageFlags usage) const {
    VkImageFormatProperties properties{};
    const auto query = state->InstanceFunction<PFN_vkGetPhysicalDeviceImageFormatProperties>("vkGetPhysicalDeviceImageFormatProperties");
    return query(state->physical, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage, 0, &properties) == VK_SUCCESS;
}

ShaderRecompiler::SpirvTarget VulkanDevice::Target() const {
    require(state->shaderProfile != nullptr, "shader device profile is unavailable");
    auto target = state->shaderProfile->Target();
    target.float32FlushToZero = state->float32FlushToZero;
    target.fusedExactArithmetic = state->fusedExactArithmetic;
    target.float32DenormPreserve = state->float32DenormPreserve;
    return target;
}

Graphics::SampleOrder VulkanDevice::HostSampleOrder(const Graphics::State& graphics) const {
    if (!graphics.sampleLocationGrid) return {};
    return Graphics::HostSampleLocationsFor(graphicsContext(), graphics).order;
}

ShaderRecompiler::SpirvTarget VulkanDevice::buildTarget() const {
    const auto& limits = state->properties.limits;
    ShaderRecompiler::SpirvTarget target{VK_API_VERSION_1_1, state->meshShader ? 0x00010400u : 0x00010300u, state->subgroup.subgroupSize, ShaderRecompiler::BdaAbi::Version, state->capabilities, state->spirvExtensions, false, {limits.maxComputeWorkGroupSize[0], limits.maxComputeWorkGroupSize[1], limits.maxComputeWorkGroupSize[2]}, limits.maxComputeWorkGroupInvocations, limits.maxComputeSharedMemorySize, {}, {}};
    target.fragmentShaderBarycentricEnabled = state->fragmentShaderBarycentric;
    target.nonConstantImageOffsets = state->maintenance8;
    target.narrowSubgroupClock = state->narrowSubgroupClock;
    target.float64DenormPreserve = state->float64DenormPreserve;
    target.float16DenormPreserve = state->float16DenormPreserve;
    target.spannedWaves = state->spannedWaves;
    target.depthRangeRemap = !state->depthRangeUnrestricted;
    target.fixedPushSlots = state->graphicsPipelineLibrary;
    target.registrationModules = state->graphicsPipelineLibrary;
    target.robustBufferAccess2 = state->robustBufferAccess2;
    target.waveLdsMemoryBarriers = !state->subgroupControlBarriers;
    target.largePixelFunctionLdsBuffer = state->driverId == VK_DRIVER_ID_NVIDIA_PROPRIETARY;
    target.pixelFunctionStorageDwords = state->pixelFunctionStorageDwords;
    target.deviceScopeLdsMemoryBarriers = !state->workgroupScopeFences;
    target.srgbDecodeFormats = state->srgbDecodeFormats & ~state->srgbCopyFormats;
    target.vertexSubgroups = (state->subgroup.supportedStages & VK_SHADER_STAGE_VERTEX_BIT) != 0;
    if (state->meshShader) {
        const auto& mesh = state->meshLimits;
        target.mesh = ShaderRecompiler::MeshTargetLimits{{mesh.maxMeshWorkGroupSize[0], mesh.maxMeshWorkGroupSize[1], mesh.maxMeshWorkGroupSize[2]}, mesh.maxMeshWorkGroupInvocations, std::min(mesh.maxMeshSharedMemorySize, mesh.maxMeshPayloadAndSharedMemorySize), mesh.maxMeshOutputVertices, mesh.maxMeshOutputPrimitives, mesh.maxMeshOutputComponents, std::min(mesh.maxMeshOutputMemorySize, mesh.maxMeshPayloadAndOutputMemorySize), mesh.meshOutputPerVertexGranularity, mesh.meshOutputPerPrimitiveGranularity};
    }
    if (state->tessellationShader) target.tessellation = ShaderRecompiler::TessellationTargetLimits{limits.maxTessellationPatchSize, limits.maxTessellationControlPerVertexInputComponents, limits.maxTessellationControlPerVertexOutputComponents, limits.maxTessellationControlPerPatchOutputComponents, limits.maxTessellationControlTotalOutputComponents, limits.maxTessellationEvaluationInputComponents, limits.maxTessellationEvaluationOutputComponents};
    return target;
}

std::optional<ShaderRecompiler::GeometryStageLimits> VulkanDevice::GeometryLimits() const {
    if (!state->geometryShader) return std::nullopt;
    const auto& limits = state->properties.limits;
    return ShaderRecompiler::GeometryStageLimits{limits.maxGeometryInputComponents, limits.maxGeometryOutputComponents, limits.maxGeometryOutputVertices, limits.maxGeometryTotalOutputComponents, limits.maxFragmentInputComponents};
}

std::uint32_t VulkanDevice::MaxFragmentInputComponents() const {
    return state->properties.limits.maxFragmentInputComponents;
}

bool VulkanDevice::PrimitiveListRestart() const {
    return state->primitiveListRestart;
}

bool VulkanDevice::DualSrcBlend() const {
    return state->dualSrcBlend;
}

bool VulkanDevice::SamplerFilterMinmax() const {
    return state->samplerFilterMinmax;
}

bool VulkanDevice::ConservativeRasterization() const {
    return state->conservativeRasterization;
}

VkShaderStageFlags VulkanDevice::SubgroupStages() const {
    return state->subgroup.supportedStages;
}

bool VulkanDevice::ProvokingVertexLast() const {
    return state->provokingVertexLast;
}

bool VulkanDevice::GraphicsPipelineLibraries() const {
    return state->graphicsPipelineLibrary;
}

Graphics::Context VulkanDevice::graphicsContext() const {
    static const bool noCache = std::getenv("APS5_NO_CONTEXT_CACHE") != nullptr;
    if (state->contextReady && !noCache) return state->context;
    return buildContext();
}

Graphics::Context VulkanDevice::buildContext() const {
    auto context = Graphics::Context{
        state->device,
        state->physical,
        state->queue,
        state->pool,
        state->deviceProc,
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFormatProperties>("vkGetPhysicalDeviceFormatProperties"),
        state->InstanceFunction<PFN_vkGetPhysicalDeviceImageFormatProperties>("vkGetPhysicalDeviceImageFormatProperties"),
        state->memoryProperties,
        state->properties.limits,
        state->tessellationShader,
        state->meshShader,
        state->meshLimits,
        state->depthClipControl,
        state->depthRangeUnrestricted,
        true,
        state->subgroup,
        state->fragmentShaderBarycentric,
        state->samplerAnisotropy,
        state->textureCompressionBC,
        state->detiler.get(),
        state->colorTransfer.get(),
        state->bufferPool,
        state->textureCache.get(),
        state->pipelineCache ? state->pipelineCache->Handle() : VK_NULL_HANDLE,
        state->depthClamp,
        state->hostImportAlignment
    };
    context.pipelineCacheSynchronization = state->pipelineCache ? &state->pipelineCache->Synchronization() : nullptr;
    context.individuallyFreeDescriptorSets = state->individuallyFreeDescriptorSets;
    context.dmaBufImport = state->dmaBufImport;
    context.sparseBinding = state->sparseBinding;
    context.sparseResidencyBuffer = state->sparseResidencyBuffer;
    context.sparseResidencyAliased = state->sparseResidencyAliased;
    context.queueFamily = state->queueFamily;
    context.sparseFamily = state->sparseFamily;
    context.sparseQueue = state->sparseQueue;
    context.sparseQueueMutex = &state->sparseQueueMutex;
    context.settleQueue = state->settlePool != VK_NULL_HANDLE ? state->settleQueue : VK_NULL_HANDLE;
    context.settlePool = state->settlePool;
    context.settleFamily = state->settleFamily;
    context.settleQueueMutex = &state->settleQueueMutex;
    context.recorder = state->recorder.get();
    context.descriptorCache = state->descriptorCache.get();
    context.vertexInputs = state->vertexInputs.get();
    context.samplerCache = state->samplerCache.get();
    context.drawIndirectFirstInstance = state->drawIndirectFirstInstance;
    context.multiDrawIndirect = state->multiDrawIndirect;
    context.dualSrcBlend = state->dualSrcBlend;
    context.depthBounds = state->depthBounds;
    context.depthBiasClamp = state->depthBiasClamp;
    context.samplerFilterMinmax = state->samplerFilterMinmax;
    context.nonSeamlessCubeMap = state->nonSeamlessCubeMap;
    context.mergedCoverageSampleCounts = state->mergedCoverageSampleCounts;
    context.truncatedCoverageSampleCounts = state->truncatedCoverageSampleCounts;
    context.mixedAttachmentSamples = state->mixedAttachmentSamples;
    context.conservativeRasterization = state->conservativeRasterization;
    context.meshWave32 = state->meshWave32;
    context.provokingVertexLast = state->provokingVertexLast;
    context.graphicsPipelineLibrary = state->graphicsPipelineLibrary;
    context.pipelineLinkTimeOptimization = state->pipelineLinkTimeOptimization;
    context.drawInputsInPlace = state->drawInputsInPlace;
    context.discreteGpu = state->properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    context.provokingVertexModePerPipeline = state->provokingVertexModePerPipeline;
    context.drawIndirectCount = state->drawIndirectCount;
    context.occlusionQueryPrecise = state->occlusionQueryPrecise;
    context.emptyBuffer = state->emptyBuffer ? state->emptyBuffer->Handle() : VK_NULL_HANDLE;
    context.copiedWriters = state->copiedWriters.get();
    context.functions = state->functionsReady ? &state->deviceFunctions : nullptr;
    context.descriptorIndexing = state->descriptorIndexing;
    context.descriptorIndexingLimits = state->descriptorIndexingProperties;
    context.imageInt64Atomics = state->imageInt64Atomics;
    context.runtimeDescriptorArray = state->runtimeDescriptorArray;
    context.partiallyBound = state->descriptorBindingPartiallyBound;
    context.variableDescriptorCount = state->descriptorBindingVariableDescriptorCount;
    context.sampledImageUpdateAfterBind = state->sampledImageUpdateAfterBind;
    context.imageTableCapacity = ImageTableCapacity(context);
    context.samplerTableCapacity = SamplerTableCapacity(context);
    context.imageTableUpdateAfterBind = ImageTableUpdateAfterBind(context);
    context.samplerTableUpdateAfterBind = SamplerTableUpdateAfterBind(context);
    context.geometryShader = state->geometryShader;
    context.largePoints = state->largePoints;
    context.sampleRateShading = state->sampleRateShading;
    context.fillModeNonSolid = state->fillModeNonSolid;
    context.maintenance5 = state->maintenance5;
    context.storageImageMultisample = state->storageImageMultisample;
    context.storageImageWriteWithoutFormat = state->storageImageWriteWithoutFormat;
    context.shaderClipDistance = state->shaderClipDistance;
    context.shaderCullDistance = state->shaderCullDistance;
    context.shaderTerminateInvocation = state->shaderTerminateInvocation;
    context.nullDescriptors = state->shaderProfile != nullptr && state->shaderProfile->NullDescriptors();
    context.bufferInt64Atomics = state->bufferInt64Atomics;
    context.placeholderImages = state->placeholderImages.get();
    context.viewportIndexLayer = state->viewportIndexLayer;
    context.imageFormatList = state->imageFormatList;
    context.demoteToHelperInvocation = state->demoteToHelperInvocation;
    context.earlyFragmentSampleMaskTestBeforeSampleCounting = state->earlyFragmentSampleMaskTestBeforeSampleCounting;
    context.primitiveListRestart = state->primitiveListRestart;
    context.imageViewMinLod = state->imageViewMinLod;
    context.pipelineExecutableInfo = state->pipelineExecutableInfo;
    context.srgbDecodeFormats = state->srgbDecodeFormats;
    context.srgbCopyFormats = state->srgbCopyFormats;
    context.sampleLocationSampleCounts = state->sampleLocationSampleCounts;
    context.sampleLocationGridSampleCounts = state->sampleLocationGridSampleCounts;
    context.sampleLocationGridSizes = state->sampleLocationGridSizes;
    context.shaderStencilExport = state->shaderStencilExport;
    context.memoryProperties2 =state->memoryBudget ? state->InstanceFunction<PFN_vkGetPhysicalDeviceMemoryProperties2>("vkGetPhysicalDeviceMemoryProperties2") : nullptr;
    context.singlePassStorage = state->storageImageWithoutFormat;
    context.cacheReclaim = Graphics::ReleaseTextureCaches;
    return context;
}

VulkanDevice::IndirectDrawSupport VulkanDevice::DrawIndirectSupport() const {
    return {state->drawIndirectFirstInstance, state->multiDrawIndirect, state->drawIndirectCount};
}

std::optional<std::string> VulkanDevice::KnownDrawRejection(const Graphics::State& graphics, std::span<const Graphics::CompiledShader> shaders) const {
    return Graphics::KnownValidationFailure(graphicsContext(), shaders, graphics);
}

void VulkanDevice::DepthCopyPass(const Graphics::DepthCopyPass& pass) {
    Graphics::RunDepthCopyPass(graphicsContext(), pass);
}

void VulkanDevice::ColorMetadataPass(const Graphics::ColorMetadataPass& pass) {
    Graphics::RunColorMetadataPass(graphicsContext(), pass);
}

bool VulkanDevice::PrebuildGraphicsLibraries(const Graphics::ShaderStages& stages, std::span<const Graphics::CompiledShader> shaders) {
    PerformanceTimer timing("Vulkan.GraphicsPipelinePrebuild");
    const auto context = graphicsContext();
    if (!context.graphicsPipelineLibrary || context.descriptorCache == nullptr) return false;
    Graphics::State state{};
    state.stages = stages;
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.color.format = VK_FORMAT_R8G8B8A8_UNORM;
    state.color.exportIndex = 0;
    state.colors.push_back(state.color);
    state.hasColorTarget = true;
    state.blend.colorWriteMask = 15;
    state.blends.push_back(state.blend);
    state.renderExtent = {1, 1};
    std::vector<Graphics::CompiledShader> group(shaders.begin(), shaders.end());
    if (group.size() == 1u) {
        const bool vertex = group.front().stage == ShaderRecompiler::ShaderStage::Fragment;
        static const auto stub = [](bool vertexStub) {
            auto result = std::make_shared<ShaderRecompiler::RecompileResult>();
            const std::uint32_t model = vertexStub ? 0u : 4u;
            std::vector<std::uint32_t> words{0x07230203u, 0x00010000u, 0u, 6u, 0u, 0x00020011u, 1u, 0x0003000eu, 0u, 1u, 0x0005000fu, model, 4u, 0x6e69616du, 0u};
            if (!vertexStub) words.insert(words.end(), {0x00030010u, 4u, 7u});
            words.insert(words.end(), {0x00020013u, 2u, 0x00030021u, 3u, 2u, 0x00050036u, 2u, 4u, 0u, 3u, 0x000200f8u, 5u, 0x000100fdu, 0x00010038u});
            result->spirv = std::move(words);
            result->variantId = vertexStub ? ~std::uint64_t{0} - 1u : ~std::uint64_t{0} - 2u;
            result->runtimeAbiVersion = ShaderRecompiler::RuntimeAbi::Version;
            return result;
        };
        static const auto vertexStub = stub(true);
        static const auto fragmentStub = stub(false);
        if (vertex) group.insert(group.begin(), {ShaderRecompiler::ShaderStage::Vertex, vertexStub.get(), 0u});
        else group.push_back({ShaderRecompiler::ShaderStage::Fragment, fragmentStub.get(), Graphics::PipelinePushSlotBytes});
    }
    const auto layouts = Graphics::DescribeSetLayouts(context, group);
    return Graphics::Pipeline::Prebuild(context, state, Graphics::VertexInputLayout{}, layouts.View(), group);
}

void VulkanDevice::Draw(const Graphics::State& graphics, const Pm4::DrawParameters& draw, std::span<const Graphics::CompiledShader> shaders, std::span<const Graphics::GuestMemorySnapshot> snapshots, std::shared_ptr<const DrawRecipe>* recipe) {
    startJournalReplay();
    for (const auto& shader : shaders) {
        require(shader.program != nullptr, "missing compiled shader");
        ShaderRecompiler::RuntimeAbi::RequireVersion(shader.program->runtimeAbiVersion);
    }
    // Two stdout lines per draw cost ~1.3 ms per frame of the queue-0 worker (part of it under the
    // GPU mutex); APS5_TRACE_DRAWS=1 restores them.
    static const bool trace = std::getenv("APS5_TRACE_DRAWS") != nullptr;
    if (trace) APS5_LOG_OUT("VulkanDevice::Draw indices=%u instances=%u indexSize=%u address=0x%llx shaders=%zu colorTarget=%u", draw.indexCount, draw.instanceCount, draw.indexSize, static_cast<unsigned long long>(draw.indexAddress), shaders.size(), static_cast<unsigned>(graphics.hasColorTarget));
    const auto context = graphicsContext();
    Graphics::Draw(context, graphics, draw, shaders, snapshots, recipe);
    if (trace) APS5_LOG_CHARS_OUT("VulkanDevice::Draw complete");
}

bool VulkanDevice::DrawInputsInPlace() const {
    return state->drawInputsInPlace;
}

namespace {

// The resource cache serves dispatches unless disabled; Revalidate proves a hit by texture identity,
// which needs the texture caches: without them every lookup makes a fresh object, so a hit could
// never validate and would only upload everything twice.
bool ResourceCacheEnabled() {
    static const bool noResourceCache = std::getenv("APS5_NO_RESOURCE_CACHE") != nullptr;
    static const bool noTextureCache = std::getenv("APS5_NO_TEXTURE_CACHE") != nullptr;
    return !noResourceCache && !noTextureCache;
}

// Compute templates serve dispatches whose ShaderData/FlattenedSrt words differ (the words leave
// the key; a hit refreshes the template's data buffers with the dispatch's words at the record,
// ShaderResources::RefreshData). APS5_NO_TEMPLATE_DATA_REFRESH=1 keys the words as before.
bool TemplateDataRefresh() {
    static const bool enabled = std::getenv("APS5_NO_TEMPLATE_DATA_REFRESH") == nullptr;
    return enabled;
}

bool DispatchMove() {
    static const bool enabled = std::getenv("APS5_NO_DISPATCH_MOVE") == nullptr && TemplateDataRefresh();
    return enabled;
}

std::size_t DispatchMoveBytes() {
    static const std::size_t bytes = [] {
        const char* text = std::getenv("APS5_DISPMOVE_MAX_KIB");
        return static_cast<std::size_t>(text != nullptr ? std::strtoull(text, nullptr, 10) : 1024ull) * 1024u;
    }();
    return bytes;
}

// The content key of a compute stage, naming the device: the cache is process-wide and the driver
// replaces the headless device with the windowed one while workers may still use the old one, so
// the key names the device that built the entry (its descriptor set and pooled buffers belong to it).
ResourceCache::Key DispatchContentKey(const Graphics::CompiledShader& shader, VkDevice device, bool movable = true) {
    auto key = Graphics::ShaderResources::ContentKey(shader, !TemplateDataRefresh(), movable && DispatchMove());
    const auto deviceHandle = reinterpret_cast<std::uint64_t>(device);
    key.push_back(static_cast<std::uint32_t>(deviceHandle));
    key.push_back(static_cast<std::uint32_t>(deviceHandle >> 32u));
    return key;
}

// The phases of VulkanDevice::dispatch (APS5_PROFILE_DRAW), followed by one row per image lookup
// outcome (Graphics::LookupOutcomes) on the [indirect] line.
enum DispatchPhase : std::size_t { PhaseReap, PhaseResources, PhaseResourcesInsert, PhaseResourcesComplete, PhaseResourcesImages, PhaseResourcesUpload, PhaseResourcesDescriptors, PhaseResourcesOther, PhaseResourcesALocked, PhaseResourcesAddress, PhaseResourcesAUnlocked, PhaseResourcesRevalidate, PhaseResourcesFullBuild, PhaseResourcesHookWaits, PhaseProof, PhasePipeline, PhaseDecide, PhaseArgumentRead, PhaseRecordCommands, PhaseRecordKeeps, PhaseRecordDataRefresh, PhaseRecordBind, PhaseRecordMarks, PhaseRecordCompletion, PhaseRecord, PhaseSync, DispatchPhaseCount };
constexpr std::array<const char*, DispatchPhaseCount> DispatchPhaseNames{"reap", "resources", "resources: cache insert", "resources: complete (wall)", "resources: B images", "resources: B upload", "resources: B descriptors", "resources: B bda+other", "resources: A locked (bda)", "resources: address bindings (bda)", "resources A (unlocked)", "resources: revalidate", "resources: full build (locked)", "resources: hook waits", "proof", "pipeline", "decide", "argument read", "record: commands", "record: keeps", "record: data refresh", "record: bind+dispatch", "record: marks", "record: completion", "record", "sync"};
constexpr std::size_t DispatchRows = static_cast<std::size_t>(DispatchPhaseCount) + static_cast<std::size_t>(Graphics::LookupOutcomes::Count);

const char* DispatchRowName(std::size_t row) {
    if (row < DispatchPhaseCount) return DispatchPhaseNames[row];
    static const std::array<std::string, Graphics::LookupOutcomes::Count> imageRows = [] {
        std::array<std::string, Graphics::LookupOutcomes::Count> rows;
        for (std::size_t kind = 0; kind < Graphics::LookupOutcomes::Count; ++kind) rows[kind] = std::string("images: ") + Graphics::LookupOutcomes::Name(static_cast<Graphics::LookupOutcomes::Kind>(kind));
        return rows;
    }();
    return imageRows[row - DispatchPhaseCount].c_str();
}

}

struct PreparedDispatch {
    // Stage A done; stage B (Complete) runs in the dispatch under the mutex. Null when the resource
    // cache held the key's object at prepare time: the dispatch revalidates `cached` instead.
    std::shared_ptr<Graphics::ShaderResources> resources;
    std::shared_ptr<Graphics::ShaderResources> cached;
    ResourceCache::Key key;
    bool exact = false;
    // APS5_PROFILE_DRAW: stage A's time, added to the [dispatch] phase totals by the dispatch, and
    // the prepare's parts for the driver's 'prepare:' rows (PreparePhase order).
    double prepareMs = 0;
    std::uint64_t dispatchThreads = 0;
    enum PreparePhase : std::size_t { PrepareKey, PrepareFind, PreparePrecollect, PreparePresync, PrepareStageA, PreparePhaseCount };
    std::array<double, PreparePhaseCount> phaseMs{};
    // The stage-A pre-sync (see PrepareDispatch): the newest recorder serial waited for without the
    // mutex, whose batches the dispatch reaps under it before stage B (0: nothing waited for).
    std::uint64_t presyncSerial = 0;
    std::shared_ptr<ComputePipelineObjects> objects;
    std::vector<std::uint32_t> describedLayoutKey;
};

struct RecipeHit {
    std::shared_ptr<const Recipe> recipe;
    // The recipe's template and pipeline objects, locked by the pre-check.
    std::shared_ptr<Graphics::ShaderResources> resources;
    std::shared_ptr<ComputePipelineObjects> objects;
    // The pre-sync's serial (0: nothing waited for), reaped by DispatchRecipe.
    std::uint64_t presyncSerial = 0;
    bool indirect = false;
    // APS5_PROFILE_DRAW: the pre-check's time (the driver's 'recipe pre-check' row).
    double precheckMs = 0;
};

namespace {

// APS5_PROFILE_DRAW: the pre-syncs made and the time they waited (unlocked), as [presync] every 10 s.
struct PresyncCounters {
    std::atomic<std::uint64_t> checked{0};
    std::atomic<std::uint64_t> presyncs{0};
    std::atomic<std::uint64_t> waitedUs{0};
    std::atomic<std::uint64_t> keyChecks{0};
    std::atomic<std::uint64_t> keyWaits{0};
    std::atomic<std::int64_t> lastReport{0};
};

PresyncCounters& Presyncs() {
    static PresyncCounters counters;
    return counters;
}

// A cached object's Revalidate collects the write watch over its surfaces under the mutex; the
// walk is made before the lock instead, so those collects are memo hits (as stage A's precollect
// does for a build). APS5_NO_CACHED_PRECOLLECT=1 leaves the walks under the lock.
bool CachedPrecollect() {
    static const bool cachedPrecollect = std::getenv("APS5_NO_CACHED_PRECOLLECT") == nullptr;
    return cachedPrecollect;
}

// APS5_NO_PRESYNC=1 disables the stage-A pre-sync.
bool NoPresync() {
    static const bool noPresync = std::getenv("APS5_NO_PRESYNC") != nullptr;
    return noPresync;
}

// Debug aid: APS5_SYNC_DISPATCH=1 waits for every dispatch, as before batching.
bool SyncEachDispatch() {
    static const bool syncEachDispatch = std::getenv("APS5_SYNC_DISPATCH") != nullptr;
    return syncEachDispatch;
}

// Debug aid: APS5_TRACE_DISPATCH_IO prints every dispatch's resources (after write-back when synced).
bool TraceDispatchIo() {
    static const bool traceIo = std::getenv("APS5_TRACE_DISPATCH_IO") != nullptr;
    return traceIo;
}

void WatchMemory(std::uint64_t programAddress) {
    struct Range {
        std::uint64_t address = 0;
        std::uint64_t bytes = 0;
    };
    static const std::vector<Range> ranges = [] {
        std::vector<Range> parsed;
        const char* text = std::getenv("APS5_WATCH_MEMORY");
        if (text == nullptr) return parsed;
        while (true) {
            char* end = nullptr;
            const auto address = std::strtoull(text, &end, 16);
            const auto bytes = *end == ':' ? std::strtoull(end + 1, &end, 16) : 0ull;
            if (address == 0 || bytes == 0 || (*end != ',' && *end != '\0')) throw std::runtime_error("APS5_WATCH_MEMORY takes <hex address>:<hex bytes>[,...]");
            parsed.push_back({address, bytes});
            if (*end == '\0') break;
            text = end + 1;
        }
        if (!SyncEachDispatch()) throw std::runtime_error("APS5_WATCH_MEMORY needs APS5_SYNC_DISPATCH=1");
        return parsed;
    }();
    if (ranges.empty()) return;
    static AgcDriver::Mutex mutex;
    static std::deque<std::pair<std::uint64_t, std::vector<std::uint8_t>>> history;
    static bool written = false;
    std::lock_guard lock(mutex);
    if (written) return;
    static std::vector<std::uint64_t> addresses;
    if (addresses.empty()) {
        std::vector<std::uint64_t> found;
        for (const auto& range : ranges) {
            std::uint64_t address = 0;
            for (std::uint64_t high = 0; high < 16 && address == 0; ++high) {
                const auto candidate = (range.address & 0xfffffffffull) | (high << 36u);
                if (GuestMemory::Accessible(reinterpret_cast<const void*>(candidate), static_cast<std::size_t>(range.bytes))) address = candidate;
            }
            if (address == 0) return;
            found.push_back(address);
        }
        addresses = std::move(found);
        for (std::size_t i = 0; i < ranges.size(); ++i) std::fprintf(stderr, "[watch] watching 0x%llx+0x%llx\n", static_cast<unsigned long long>(addresses[i]), static_cast<unsigned long long>(ranges[i].bytes));
    }
    std::vector<std::uint8_t> contents;
    for (std::size_t i = 0; i < ranges.size(); ++i) {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(addresses[i]);
        if (!GuestMemory::Accessible(bytes, static_cast<std::size_t>(ranges[i].bytes))) return;
        contents.insert(contents.end(), bytes, bytes + ranges[i].bytes);
    }
    if (history.empty() || history.back().second != contents) {
        history.emplace_back(programAddress, std::move(contents));
        if (history.size() > 64) history.pop_front();
    }
    if (!Graphics::LoopGuardTripped()) return;
    written = true;
    for (std::size_t i = 0; i < history.size(); ++i) {
        char name[64];
        std::snprintf(name, sizeof(name), "watch_%02zu_%llx.bin", i, static_cast<unsigned long long>(history[i].first));
        std::ofstream(name, std::ios::binary).write(reinterpret_cast<const char*>(history[i].second.data()), static_cast<std::streamsize>(history[i].second.size()));
    }
    std::fprintf(stderr, "[watch] loop guard tripped: %zu contents written (watch_*.bin, oldest first)\n", history.size());
}

// The [dispatch] phases for indirect dispatches alone, every 10 s on an [indirect] line: what the
// 'indirect' GpuMutex hold ([lock] line) spends its time on inside the device call (the label
// record before it is timed in the driver, [labels] line), with the longest hold seen and the
// copied-writer lists the indirect decision scans. All under the mutex, like the rest.
struct IndirectHold {
    std::array<double, DispatchRows> phaseMs{};
    // Rows with a count (the cache insert's evictions, the image lookups by outcome).
    std::array<std::uint64_t, DispatchRows> phaseCounts{};
    std::uint64_t count = 0;
    double totalMs = 0;
    double maxMs = 0;
    // The phases and lookup outcomes of the longest call, so the maximum is named.
    std::string maxPhases;
    std::uint64_t writersScanned = 0;
    std::uint64_t drawWritersScanned = 0;
    std::size_t maxWriters = 0;
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
};

// The running totals of the device call (APS5_PROFILE_DRAW), shared by dispatch, DispatchRecipe
// and recordDispatch; mutated under GuestMemory::GpuMutex like the calls themselves. The
// [rescache] and [vk] counters: template hits by outcome, vkGetDeviceProcAddr lookups made inside
// the device call (this thread's), the pre-dispatch barriers recorded or skipped (Recorder::Commands).
struct DispatchCounters {
    std::array<double, DispatchRows> phaseTotals{};
    std::uint64_t profiledDispatches = 0;
    IndirectHold indirectHold;
    std::uint64_t cacheHits = 0, cacheMisses = 0, cacheInvalidated = 0, templateRefreshed = 0, templateSameWords = 0, templateByNumber = 0;
    double templateRevalidateMs = 0;
    std::uint64_t procLookups = 0, preBarriersRecorded = 0, preBarriersSkipped = 0;
    std::chrono::steady_clock::time_point cacheReport = std::chrono::steady_clock::now();
};

DispatchCounters& Dispatches() {
    static DispatchCounters counters;
    return counters;
}

// The [recipe] line (APS5_PROFILE_DRAW, every 10 s), dispatch and indirect rows apart. The
// pre-check runs without the mutex, so the counters are atomic.
enum RecipeMiss : std::size_t { MissNoRecipe, MissDevice, MissTemplateGone, MissObjectsGone, MissNotRecordable, RecipeMissCount };
constexpr std::array<const char*, RecipeMissCount> RecipeMissNames{"no recipe", "device", "template gone", "objects gone", "not recordable"};
// The draw rows name the miss reasons of DrawWithRecipe (a target or a pipeline gone is the
// objects-gone slot).
constexpr std::array<const char*, RecipeMissCount> DrawRecipeMissNames{"no recipe", "device", "template gone", "targets gone", "not recordable"};
constexpr std::array<const char*, static_cast<std::size_t>(Graphics::ShaderResources::ProofPath::Count)> ProofPathNames{"fast", "T1 refreshed", "full"};
constexpr std::array<const char*, static_cast<std::size_t>(Graphics::ShaderResources::ProofFailure::Count)> ProofFailureNames{"none", "imports", "evicted", "pending", "changed", "keys", "other"};

struct RecipeCounters {
    struct Kind {
        std::atomic<std::uint64_t> hits{0};
        std::array<std::atomic<std::uint64_t>, RecipeMissCount> precheckMisses{};
        std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(Graphics::ShaderResources::ProofPath::Count)> proofPaths{};
        std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(Graphics::ShaderResources::ProofFailure::Count)> rebuilds{};
        std::atomic<std::uint64_t> rebuildDevice{0};
        std::atomic<std::uint64_t> presyncsFromRecipe{0}, presyncsRecomputed{0}, presyncWaits{0};
        std::atomic<std::uint64_t> dataRefreshed{0}, dataSkipped{0}, refreshByWords{0};
        std::atomic<std::uint64_t> restarts{0}, attaches{0}, verified{0}, verifyReplaced{0};
        std::atomic<std::uint64_t> precheckNs{0}, proofNs{0}, recordNs{0};
    };
    std::array<Kind, 3> kinds;
    std::atomic<std::int64_t> lastReport{0};
};

RecipeCounters& Recipes() {
    static RecipeCounters counters;
    return counters;
}

constexpr std::size_t MoveRefusals = static_cast<std::size_t>(Graphics::ShaderResources::MoveRefusal::Count);

struct DispatchMoveCounters {
    std::atomic<std::uint64_t> checked{0}, exact{0}, moved{0}, buffers{0}, bytes{0};
    std::atomic<std::uint64_t> rebuilt{0}, kept{0}, served{0};
    std::array<std::atomic<std::uint64_t>, 5> sizes{};
    std::array<std::atomic<std::uint64_t>, MoveRefusals> refused{};
    std::array<std::atomic<std::uint64_t>, MoveRefusals> filtered{};
    std::atomic<std::int64_t> lastReport{0};
};

DispatchMoveCounters& DispatchMoves() {
    static DispatchMoveCounters counters;
    return counters;
}

void reportDispatchMove() {
    auto& moves = DispatchMoves();
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    auto last = moves.lastReport.load();
    if (nowMs - last < 10000 || !moves.lastReport.compare_exchange_strong(last, nowMs)) return;
    const auto take = [](std::atomic<std::uint64_t>& counter) { return static_cast<unsigned long long>(counter.exchange(0, std::memory_order_relaxed)); };
    const auto list = [&](std::array<std::atomic<std::uint64_t>, MoveRefusals>& counters) {
        std::string text;
        unsigned long long total = 0;
        char item[80];
        for (std::size_t i = 1; i < MoveRefusals; ++i) {
            const auto count = take(counters[i]);
            total += count;
            if (count == 0) continue;
            std::snprintf(item, sizeof(item), "%s %s %llu", text.empty() ? "" : ",", Graphics::ShaderResources::MoveRefusalName(static_cast<Graphics::ShaderResources::MoveRefusal>(i)), count);
            text += item;
        }
        std::snprintf(item, sizeof(item), "%llu (", total);
        return std::string(item) + text + ")";
    };
    const auto checked = take(moves.checked);
    const auto exact = take(moves.exact);
    const auto moved = take(moves.moved);
    const auto buffers = take(moves.buffers);
    const auto bytes = take(moves.bytes);
    const auto rebuilt = take(moves.rebuilt);
    const auto kept = take(moves.kept);
    const auto served = take(moves.served);
    unsigned long long sizes[5];
    for (std::size_t i = 0; i < 5; ++i) sizes[i] = take(moves.sizes[i]);
    const auto refused = list(moves.refused);
    const auto filtered = list(moves.filtered);
    AgcDriver::ProfilePrint_nid_no_patch("[dispmove] (10 s) %llu template hits checked: %llu with every read-only buffer where it was built, %llu with %llu moved ones (%.2f MiB snapshotted; per dispatch up to 1 KiB %llu, 16 KiB %llu, 256 KiB %llu, 4 MiB %llu, more %llu); refused %s at stage A (the hit was dropped before the record), %s at record; after a refusal: %llu rebuilt, %llu inserted beside the movable template (kept), %llu served by an exact entry\n", checked, exact, moved, buffers, static_cast<double>(bytes) / 1048576.0, sizes[0], sizes[1], sizes[2], sizes[3], sizes[4], filtered.c_str(), refused.c_str(), rebuilt, kept, served);
}

void noteDispatchMoveSize(std::size_t bytes) {
    auto& moves = DispatchMoves();
    const std::size_t bucket = bytes <= 1024 ? 0 : bytes <= 16384 ? 1 : bytes <= 262144 ? 2 : bytes <= 4194304 ? 3 : 4;
    moves.sizes[bucket].fetch_add(1, std::memory_order_relaxed);
}

void reportRecipes() {
    auto& recipes = Recipes();
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    auto last = recipes.lastReport.load();
    if (nowMs - last < 10000 || !recipes.lastReport.compare_exchange_strong(last, nowMs)) return;
    const auto take = [](std::atomic<std::uint64_t>& counter) { return static_cast<unsigned long long>(counter.exchange(0, std::memory_order_relaxed)); };
    for (std::size_t kind = 0; kind < 3; ++kind) {
        auto& counters = recipes.kinds[kind];
        const auto hits = take(counters.hits);
        std::string misses, proofs, rebuilds;
        char text[64];
        for (std::size_t i = 0; i < RecipeMissCount; ++i) {
            std::snprintf(text, sizeof(text), "%s%s %llu", i == 0 ? "" : ", ", (kind == 2 ? DrawRecipeMissNames : RecipeMissNames)[i], take(counters.precheckMisses[i]));
            misses += text;
        }
        for (std::size_t i = 0; i < ProofPathNames.size(); ++i) {
            std::snprintf(text, sizeof(text), "%s%s %llu", i == 0 ? "" : ", ", ProofPathNames[i], take(counters.proofPaths[i]));
            proofs += text;
        }
        for (std::size_t i = 1; i < ProofFailureNames.size(); ++i) {
            std::snprintf(text, sizeof(text), "%s%s %llu", i == 1 ? "" : ", ", ProofFailureNames[i], take(counters.rebuilds[i]));
            rebuilds += text;
        }
        std::snprintf(text, sizeof(text), ", device %llu", take(counters.rebuildDevice));
        rebuilds += text;
        const auto perHit = [&](std::atomic<std::uint64_t>& ns) { return hits != 0 ? static_cast<double>(take(ns)) / 1000.0 / static_cast<double>(hits) : 0.0; };
        const auto precheckUs = perHit(counters.precheckNs);
        const auto proofUs = perHit(counters.proofNs);
        const auto recordUs = perHit(counters.recordNs);
        AgcDriver::ProfilePrint_nid_no_patch("[recipe] %s (10 s): hits %llu; pre-check misses: %s; proofs: %s; rebuilds: %s; presyncs from the recipe %llu (recomputed %llu, waited %llu); data refreshes recorded %llu / skipped by hash %llu (%llu decided by words: data hits); restarts %llu, attaches %llu; us per hit: pre-check %.1f, proof %.1f, record %.1f; verified %llu (object replaced %llu)\n", kind == 0 ? "dispatch" : kind == 1 ? "indirect" : "draw", hits, misses.c_str(), proofs.c_str(), rebuilds.c_str(), take(counters.presyncsFromRecipe), take(counters.presyncsRecomputed), take(counters.presyncWaits), take(counters.dataRefreshed), take(counters.dataSkipped), take(counters.refreshByWords), take(counters.restarts), take(counters.attaches), precheckUs, proofUs, recordUs, take(counters.verified), take(counters.verifyReplaced));
    }
}

// The phases of one device call (APS5_PROFILE_DRAW): this call's rows (formatted only for a slow
// call or a new [indirect] maximum) and the running totals of Dispatches(); `indirect` charges the
// [indirect] rows too (cleared when a CPU-resolved indirect dispatch continues as a direct one).
struct DispatchTimer {
    bool profile;
    bool indirect;
    std::size_t spirvWords;
    std::uint64_t programAddress;
    std::array<double, DispatchRows> callMs{};
    std::chrono::steady_clock::time_point start;
    std::chrono::steady_clock::time_point phaseStart;
    DispatchTimer(bool profile, bool indirect, std::size_t spirvWords, std::uint64_t programAddress) : profile(profile), indirect(indirect), spirvWords(spirvWords), programAddress(programAddress), start(std::chrono::steady_clock::now()), phaseStart(start) {}
    void add(DispatchPhase which, double ms) {
        if (!profile) return;
        auto& d = Dispatches();
        callMs[which] += ms;
        d.phaseTotals[which] += ms;
        if (indirect) d.indirectHold.phaseMs[which] += ms;
    }
    void phase(DispatchPhase which) {
        if (!profile) return;
        const auto now = std::chrono::steady_clock::now();
        const auto ms = std::chrono::duration<double, std::milli>(now - phaseStart).count();
        add(which, ms);
        phaseStart = now;
        // Slow phases are reported as they finish, so a dispatch that never completes shows where it is.
        if (ms > 1000) AgcDriver::ProfilePrint_nid_no_patch("[dispatch] %s took %.0f ms (%zu words, program 0x%llx)\n", DispatchRowName(which), ms, spirvWords, static_cast<unsigned long long>(programAddress));
    }
    void restart() { phaseStart = std::chrono::steady_clock::now(); }
    // The call's phases as text, only when wanted: a slow call, or a new [indirect] maximum.
    std::string formatCall() const {
        std::string text;
        char row[96];
        for (std::size_t i = 0; i < DispatchRows; ++i) {
            if (callMs[i] == 0) continue;
            std::snprintf(row, sizeof(row), " %s=%.2fms", DispatchRowName(i), callMs[i]);
            text += row;
        }
        return text;
    }
    // Every 1000 profiled dispatches: the totals per phase.
    static void countDispatch(bool profile) {
        if (!profile) return;
        auto& d = Dispatches();
        if (++d.profiledDispatches % 1000 != 0) return;
        std::string report;
        for (std::size_t row = 0; row < DispatchPhaseCount; ++row) {
            if (d.phaseTotals[row] != 0) report += " " + std::string(DispatchRowName(row)) + "=" + std::to_string(static_cast<long long>(d.phaseTotals[row] / 1000)) + "s";
        }
        AgcDriver::ProfilePrint_nid_no_patch("[dispatch] %llu dispatches, phase totals:%s\n", static_cast<unsigned long long>(d.profiledDispatches), report.c_str());
    }
    // The call's end: the proc lookups it made, the slow-call report, the [indirect] hold
    // accounting (`indirectHold`: the call was an indirect dispatch, CPU-resolved or not) and line.
    void finish(std::uint64_t lookupsBefore, const char* groupsText, bool indirectHold) const {
        if (!profile) return;
        auto& d = Dispatches();
        d.procLookups += Graphics::DeviceProcLookups() - lookupsBefore;
        const auto now = std::chrono::steady_clock::now();
        const auto totalMs = std::chrono::duration<double, std::milli>(now - start).count();
        if (totalMs > 100) AgcDriver::ProfilePrint_nid_no_patch("[dispatch] %s %zu words:%s\n", groupsText, spirvWords, formatCall().c_str());
        if (!indirectHold) return;
        auto& hold = d.indirectHold;
        ++hold.count;
        hold.totalMs += totalMs;
        if (totalMs > hold.maxMs) {
            hold.maxMs = totalMs;
            hold.maxPhases = formatCall();
        }
        if (now - hold.lastReport <= std::chrono::seconds(10)) return;
        hold.lastReport = now;
        std::string report;
        for (std::size_t i = 0; i < DispatchRows; ++i) {
            if (hold.phaseMs[i] == 0 && hold.phaseCounts[i] == 0) continue;
            char text[112];
            if (hold.phaseCounts[i] != 0) std::snprintf(text, sizeof(text), " %s %llux %.0f ms", DispatchRowName(i), static_cast<unsigned long long>(hold.phaseCounts[i]), hold.phaseMs[i]);
            else std::snprintf(text, sizeof(text), " %s %.0f ms", DispatchRowName(i), hold.phaseMs[i]);
            report += text;
        }
        AgcDriver::ProfilePrint_nid_no_patch("[indirect] %llu indirect dispatches spent %.0f ms inside the device call (10 s; max %.1f ms:%s), by phase (the 'resources: ...' rows split 'resources'; 'hook waits' overlaps them; 'images: ...' rows are the lookups by outcome, count x ms, inside 'B images'):%s; copied-writer lists scanned per decision: dispatch avg %.1f (max %zu), draw avg %.1f\n", static_cast<unsigned long long>(hold.count), hold.totalMs, hold.maxMs, hold.maxPhases.c_str(), report.c_str(), hold.count != 0 ? static_cast<double>(hold.writersScanned) / hold.count : 0.0, hold.maxWriters, hold.count != 0 ? static_cast<double>(hold.drawWritersScanned) / hold.count : 0.0);
        hold.phaseMs = {};
        hold.phaseCounts = {};
        hold.count = 0;
        hold.totalMs = 0;
        hold.maxMs = 0;
        hold.maxPhases.clear();
        hold.writersScanned = 0;
        hold.drawWritersScanned = 0;
        hold.maxWriters = 0;
    }
};

}

struct RecordedDispatch {
    const Graphics::Context* context;
    const Graphics::CompiledShader* shader;
    std::shared_ptr<Graphics::ShaderResources> resources;
    std::shared_ptr<ComputePipelineObjects> objects;
    VkShaderStageFlags pushStages;
    const std::array<std::byte, Graphics::PipelinePushConstantBytes>* pushBytes;
    std::uint32_t x, y, z;
    // The DISPATCH_INDIRECT arguments (0: direct) and their host import when the GPU reads them.
    std::uint64_t arguments;
    const Graphics::HostImport* argumentImport;
    std::uint64_t programAddress;
    // What decides the template's data refresh: None (the object was built by this call), Words
    // (a template hit: RefreshData compares every buffer's words), Hash (a recipe: the template's
    // DataWordsHash against the recipe's, RefreshData only when they differ).
    enum class DataRefresh { None, Words, Hash };
    DataRefresh dataRefresh;
    std::uint64_t dataWordsHash;
    // A template hit's revalidate time, charged to the [rescache] refresh accounting.
    double revalidateMs;
    DispatchTimer* timer;
    std::optional<std::pair<std::uint64_t, std::size_t>> htileClear;
    // Whether the data refresh recorded anything.
    bool refreshed = false;
    bool resourceKeyMatched = false;
    std::shared_ptr<Graphics::ShaderResources::DrawBindings> bindings{};
    std::uint64_t cpuArguments = 0;
    int cpuReason = 0;
    std::array<std::uint32_t, 3> threadGroup{};
    std::array<std::uint32_t, 3> threadLimit{};
};

struct DispatchCompletion {
    const Graphics::Context& context;
    const std::array<Graphics::CompiledShader, 1>& shaders;
    VkShaderStageFlags pushStages;
    const std::array<std::byte, Graphics::PipelinePushConstantBytes>& pushBytes;
    const std::shared_ptr<ComputePipelineObjects>& objects;
    const std::shared_ptr<Graphics::ShaderResources>& resources;
    const ResourceCache::Key& contentKey;
    bool cacheable;
    bool fromCache;
    double revalidateMs;
    const std::shared_ptr<Graphics::ShaderResources::DrawBindings>& overlay;
    std::uint64_t lookupsBefore;
    DispatchTimer& timer;
    PerformanceTimer& timing;
    char* groupsText;
};

VkDevice VulkanDevice::Device() const {
    return state->device;
}

VulkanDevice::DispatchMoveCount VulkanDevice::DispatchMoveCounts() {
    auto& moves = DispatchMoves();
    DispatchMoveCount count{moves.checked.load(), moves.exact.load(), moves.moved.load(), 0, 0, moves.rebuilt.load(), moves.kept.load(), moves.served.load()};
    for (std::size_t i = 1; i < MoveRefusals; ++i) {
        count.refused += moves.refused[i].load();
        count.filtered += moves.filtered[i].load();
    }
    return count;
}

bool VulkanDevice::DispatchRecipes() {
    static const bool noDispatchRecipe = std::getenv("APS5_NO_DISPATCH_RECIPE") != nullptr;
    return !noDispatchRecipe;
}

bool VulkanDevice::VerifyRecipes() {
    static const bool verifyRecipe = std::getenv("APS5_VERIFY_RECIPE") != nullptr;
    return verifyRecipe;
}

bool VulkanDevice::TemplateDataRefresh() {
    return AgcDriver::TemplateDataRefresh();
}

void VulkanDevice::NoteRecipe(RecipeEvent event, bool indirect) {
    NoteRecipe(event, indirect ? RecipeKind::Indirect : RecipeKind::Dispatch);
}

void VulkanDevice::NoteRecipe(RecipeEvent event, RecipeKind kind) {
    auto& counters = Recipes().kinds[static_cast<std::size_t>(kind)];
    (event == RecipeEvent::Restart ? counters.restarts : counters.attaches).fetch_add(1, std::memory_order_relaxed);
}

void VulkanDevice::NoteDrawRecipeMiss(DrawRecipePrecheck miss) {
    auto& counters = Recipes().kinds[static_cast<std::size_t>(RecipeKind::Draw)];
    counters.precheckMisses[miss == DrawRecipePrecheck::NoRecipe ? MissNoRecipe : MissDevice].fetch_add(1, std::memory_order_relaxed);
}

RecipeOutcome VulkanDevice::DrawFromRecipe(const Graphics::State& graphics, const Pm4::DrawParameters& draw, std::span<const Graphics::CompiledShader> shaders, std::span<const Graphics::GuestMemorySnapshot> snapshots, const std::shared_ptr<const DrawRecipe>& recipe) {
    PerformanceTimer timing("Vulkan.DrawFromRecipe");
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    auto& counters = Recipes().kinds[static_cast<std::size_t>(RecipeKind::Draw)];
    // R10: the recipe names the device it was built on (the driver may have replaced the device
    // between the hit and the lock).
    if (recipe->device != state->device) {
        counters.rebuildDevice.fetch_add(1, std::memory_order_relaxed);
        return RecipeOutcome::Rebuild;
    }
    const auto context = graphicsContext();
    const auto outcome = Graphics::DrawWithRecipe(context, graphics, draw, shaders, snapshots, *recipe);
    if (!outcome.recorded) {
        switch (outcome.miss) {
            case Graphics::DrawRecipeMiss::NotRecordable: counters.precheckMisses[MissNotRecordable].fetch_add(1, std::memory_order_relaxed); break;
            case Graphics::DrawRecipeMiss::TargetGone:
            case Graphics::DrawRecipeMiss::ObjectsGone: counters.precheckMisses[MissObjectsGone].fetch_add(1, std::memory_order_relaxed); break;
            case Graphics::DrawRecipeMiss::TemplateGone: counters.precheckMisses[MissTemplateGone].fetch_add(1, std::memory_order_relaxed); break;
            case Graphics::DrawRecipeMiss::Proof: counters.rebuilds[static_cast<std::size_t>(outcome.proof.failure)].fetch_add(1, std::memory_order_relaxed); break;
            default: break;
        }
        return RecipeOutcome::Rebuild;
    }
    counters.proofPaths[static_cast<std::size_t>(outcome.proof.path)].fetch_add(1, std::memory_order_relaxed);
    counters.hits.fetch_add(1, std::memory_order_relaxed);
    if (profile) {
        counters.proofNs.fetch_add(static_cast<std::uint64_t>(outcome.proofUs * 1000.0), std::memory_order_relaxed);
        counters.recordNs.fetch_add(static_cast<std::uint64_t>(outcome.recordUs * 1000.0), std::memory_order_relaxed);
        reportRecipes();
    }
    return RecipeOutcome::Recorded;
}

std::uint64_t VulkanDevice::presync(std::span<const Graphics::PresyncSurface> surfaces) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    auto& counters = Presyncs();
    bool overlaps = false;
    for (const auto& surface : surfaces) {
        const auto bytes = static_cast<std::size_t>(surface.bytes);
        if (surface.dccKeys) counters.keyChecks.fetch_add(1, std::memory_order_relaxed);
        overlaps = overlaps || (surface.dccKeys ? Graphics::DccKeyWritersPending(surface.address, bytes) : Graphics::Recorder::SnapshotWriteOverlaps(surface.address, bytes));
    }
    std::uint64_t serial = 0;
    bool texelsPending = false;
    if (overlaps) {
        // No reap here (SubmitAndEpoch's would run completions, which a compute worker must not do
        // on queue 0's behalf, see SubmitRecorded): the dispatch reaps after the wait.
        std::lock_guard lock(GuestMemory::GpuMutex());
        if (state->recorder) {
            bool open = false;
            for (const auto& surface : surfaces) {
                const auto bytes = static_cast<std::size_t>(surface.bytes);
                if (surface.dccKeys && !Graphics::DccKeyWriterNeedsSync(surface.address, bytes)) continue;
                const auto info = state->recorder->DescribePendingWrite(surface.address, bytes);
                if (!info.has_value()) continue;
                open = open || info->open;
                serial = std::max(serial, info->serial);
                texelsPending = texelsPending || !surface.dccKeys;
            }
            if (open) serial = state->recorder->SubmitAndEpoch();
        }
    }
    if (serial != 0) {
        const auto waitStart = std::chrono::steady_clock::now();
        WaitRecorded(serial);
        counters.presyncs.fetch_add(1, std::memory_order_relaxed);
        if (!texelsPending) counters.keyWaits.fetch_add(1, std::memory_order_relaxed);
        counters.waitedUs.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - waitStart).count()), std::memory_order_relaxed);
    }
    if (profile) {
        const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        auto last = counters.lastReport.load();
        if (nowMs - last >= 10000 && counters.lastReport.compare_exchange_strong(last, nowMs)) {
            const auto keyWaitCounts = Graphics::KeyWaitCounts();
            AgcDriver::ProfilePrint_nid_no_patch("[presync] %llu dispatches checked, %llu pre-syncs waited %.0f ms (cumulative); key ranges %llu checked, %llu pre-syncs that only the keys required; WaitForKeyWriters %llu calls: %llu without a pending writer, %llu known keys, %llu fills leaving uncompressed, %llu synced\n", static_cast<unsigned long long>(counters.checked.load()), static_cast<unsigned long long>(counters.presyncs.load()), counters.waitedUs.load() / 1000.0, static_cast<unsigned long long>(counters.keyChecks.load()), static_cast<unsigned long long>(counters.keyWaits.load()), static_cast<unsigned long long>(keyWaitCounts.calls), static_cast<unsigned long long>(keyWaitCounts.noPending), static_cast<unsigned long long>(keyWaitCounts.known), static_cast<unsigned long long>(keyWaitCounts.fillsUncompressed), static_cast<unsigned long long>(keyWaitCounts.synced));
        }
    }
    return serial;
}

VulkanDevice::PresyncStatistics VulkanDevice::PresyncCounts() {
    const auto& counters = Presyncs();
    return {counters.checked.load(std::memory_order_relaxed), counters.presyncs.load(std::memory_order_relaxed), counters.keyChecks.load(std::memory_order_relaxed), counters.keyWaits.load(std::memory_order_relaxed)};
}

std::shared_ptr<PreparedDispatch> VulkanDevice::PrepareDispatch(const ShaderRecompiler::RecompileResult& shader, std::span<const Graphics::GuestMemorySnapshot> snapshots, std::uint64_t dispatchThreads, std::uint64_t programAddress) {
    ShaderRecompiler::RuntimeAbi::RequireVersion(shader.runtimeAbiVersion);
    // APS5_LOCKED_BUILD=1: the whole build under the mutex, as before the split.
    static const bool lockedBuild = std::getenv("APS5_LOCKED_BUILD") != nullptr;
    if (lockedBuild || shader.spirv.size() < 5 || shader.spirv[0] != 0x07230203u) return nullptr;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    const auto start = std::chrono::steady_clock::now();
    const Graphics::CompiledShader compute{ShaderRecompiler::ShaderStage::Compute, &shader, 0};
    const auto context = graphicsContext();
    auto prepared = std::make_shared<PreparedDispatch>();
    prepared->dispatchThreads = dispatchThreads;
    // A cached build (the map locks itself) is revalidated under the mutex by the dispatch, which
    // looks it up again by the key made here; the object stays local, only its surfaces matter
    // below. Anything inserted between here and the dispatch is simply replaced by this build.
    std::shared_ptr<Graphics::ShaderResources> cached;
    auto phaseStart = start;
    const auto phase = [&](PreparedDispatch::PreparePhase which) {
        if (!profile) return;
        const auto now = std::chrono::steady_clock::now();
        prepared->phaseMs[which] += std::chrono::duration<double, std::milli>(now - phaseStart).count();
        phaseStart = now;
    };
    if (ResourceCacheEnabled() && shader.PipelineVariantId() != 0) {
        prepared->key = DispatchContentKey(compute, context.device);
        phase(PreparedDispatch::PrepareKey);
        cached = state->resourceCache.Find(prepared->key);
        if (cached != nullptr && DispatchMove()) {
            Graphics::ShaderResources::MoveProof proof;
            if (!cached->MovedDispatchBuffers(compute, nullptr, DispatchMoveBytes(), proof).has_value()) {
                DispatchMoves().filtered[static_cast<std::size_t>(proof.refusal)].fetch_add(1, std::memory_order_relaxed);
                prepared->key = DispatchContentKey(compute, context.device, false);
                prepared->exact = true;
                cached = state->resourceCache.Find(prepared->key);
            }
        }
        prepared->cached = cached;
        phase(PreparedDispatch::PrepareFind);
    }
    if (cached == nullptr) {
        prepared->resources = std::make_shared<Graphics::ShaderResources>(context, compute, snapshots, true, dispatchThreads);
        phase(PreparedDispatch::PrepareStageA);
    }
    if (cached != nullptr && CachedPrecollect()) {
        cached->PrecollectSurfaces();
        phase(PreparedDispatch::PreparePrecollect);
    }
    // The pre-sync. Stage B's image lookups that read guest memory on the CPU (PresyncSurfaces)
    // wait, through the flush hook and under the mutex, for the recorded work writing those
    // surfaces: every other worker queues behind that wait. So the wait is made here instead,
    // without the mutex: the lock-free pending-write snapshot says whether any recorded batch
    // writes a surface; if so the mutex is taken only long enough to learn the newest such batch
    // (submitting the open one when it is that batch), the timeline wait runs unlocked, and the
    // dispatch reaps those batches at its own lock so the hook then finds nothing pending. Work
    // noted in between falls back to the locked wait as before. A cached object's Revalidate
    // repeats the same lookups when its stamps fail, so it gets the same pre-sync. Needs timeline
    // semaphores.
    if (!NoPresync() && CanWaitUnlocked()) {
        Presyncs().checked.fetch_add(1, std::memory_order_relaxed);
        const auto surfaces = cached != nullptr ? cached->PresyncSurfaces() : prepared->resources->PresyncSurfaces();
        prepared->presyncSerial = presync(surfaces);
        phase(PreparedDispatch::PreparePresync);
    }
    if (profile) prepared->prepareMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    const auto& built = cached != nullptr ? *cached : *prepared->resources;
    const std::array<Graphics::CompiledShader, 1> shaders{{compute}};
    const auto pushStages = Graphics::PushConstantStages(shaders);
    if (pushStages != 0 && state->properties.limits.maxPushConstantsSize < Graphics::ComputePushConstantBytes) return prepared;
    if (built.Layout() != VK_NULL_HANDLE) {
        const auto layouts = built.SetLayouts();
        prepared->objects = computePipeline(context, shader, layouts.main, layouts.mainKey, pushStages, programAddress);
    } else if (context.descriptorCache != nullptr) {
        const auto described = Graphics::DescribeSetLayouts(context, shaders);
        prepared->describedLayoutKey = described.mainKey;
        prepared->objects = computePipeline(context, shader, described.layouts.main, described.mainKey, pushStages, programAddress);
    }
    return prepared;
}

std::span<const double, 5> VulkanDevice::PreparePhaseMs(const PreparedDispatch& prepared) {
    static_assert(PreparedDispatch::PreparePhaseCount == 5);
    return std::span<const double, 5>(prepared.phaseMs);
}

std::shared_ptr<RecipeHit> VulkanDevice::PrepareRecipe(const std::shared_ptr<const Recipe>& recipe, bool indirect) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    auto& counters = Recipes().kinds[indirect ? 1 : 0];
    const auto start = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const auto miss = [&](RecipeMiss reason) {
        counters.precheckMisses[reason].fetch_add(1, std::memory_order_relaxed);
        return std::shared_ptr<RecipeHit>{};
    };
    if (!DispatchRecipes() || recipe == nullptr) return miss(MissNoRecipe);
    if (recipe->device != state->device) return miss(MissDevice);
    auto hit = std::make_shared<RecipeHit>();
    hit->recipe = recipe;
    hit->indirect = indirect;
    hit->resources = recipe->templateRef.lock();
    if (hit->resources == nullptr) return miss(MissTemplateGone);
    hit->objects = recipe->objects.lock();
    if (hit->objects == nullptr) return miss(MissObjectsGone);
    if (CachedPrecollect()) hit->resources->PrecollectSurfaces();
    // The pre-sync as PrepareDispatch makes it, over the recipe's surface list while the import
    // table still has the identity the list was computed under (a surface is CPU-read only while
    // its memory has no import), else over the template's current list.
    if (!NoPresync() && CanWaitUnlocked()) {
        Presyncs().checked.fetch_add(1, std::memory_order_relaxed);
        if (Graphics::HostImportsUnchanged(graphicsContext(), recipe->presyncProof)) {
            counters.presyncsFromRecipe.fetch_add(1, std::memory_order_relaxed);
            hit->presyncSerial = presync(recipe->presyncSurfaces);
        } else {
            counters.presyncsRecomputed.fetch_add(1, std::memory_order_relaxed);
            hit->presyncSerial = presync(hit->resources->PresyncSurfaces());
        }
        if (hit->presyncSerial != 0) counters.presyncWaits.fetch_add(1, std::memory_order_relaxed);
    }
    if (profile) {
        const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
        hit->precheckMs = static_cast<double>(ns) / 1e6;
        counters.precheckNs.fetch_add(static_cast<std::uint64_t>(ns), std::memory_order_relaxed);
    }
    return hit;
}

void VulkanDevice::Dispatch(const ShaderRecompiler::RecompileResult& shader, std::uint32_t x, std::uint32_t y, std::uint32_t z, std::span<const Graphics::GuestMemorySnapshot> snapshots, std::uint64_t programAddress, std::shared_ptr<PreparedDispatch> prepared, std::shared_ptr<const Recipe>* recipe, std::optional<std::pair<std::uint64_t, std::size_t>> htileClear) {
    static_cast<void>(dispatch(shader, x, y, z, 0, snapshots, programAddress, std::move(prepared), recipe, {}, htileClear));
}

VulkanDevice::IndirectOutcome VulkanDevice::DispatchIndirect(const ShaderRecompiler::RecompileResult& shader, std::uint64_t arguments, std::span<const Graphics::GuestMemorySnapshot> snapshots, std::uint64_t programAddress, std::shared_ptr<PreparedDispatch> prepared, std::shared_ptr<const Recipe>* recipe, const std::array<std::uint32_t, 3>& threadGroup) {
    return dispatch(shader, 0, 0, 0, arguments, snapshots, programAddress, std::move(prepared), recipe, threadGroup);
}

void VulkanDevice::decideIndirect(RecordedDispatch& record, IndirectOutcome& outcome, char* groupsText) {
    auto& recorder = *state->recorder;
    auto& timer = *record.timer;
    const auto& context = *record.context;
    const auto arguments = record.arguments;
    const bool threadDimensions = record.threadGroup != std::array<std::uint32_t, 3>{};
    // Decided here, after the resource build (which may retire imports and wait for recorded
    // work) and before the dispatch itself is recorded, so the import cannot be dropped before
    // the record. The build already recorded the copy-ins of its misaligned and staged regions
    // (GuestBufferMemory::recordGpuCopies), so the CPU fallback's sync below drains them in one
    // batch and records the dispatch with its copy-backs in the next; the completions the reap
    // between them runs store to the CPU outside host imports (copied regions' write-backs) or
    // onto label dwords, so none lands inside a copied range for the copy-back to roll back.
    // A label over the argument dwords (a completion-deferred one lands by a CPU store the GPU
    // read would miss; a GPU one is ordered but rare enough to share the fallback). Only those
    // dwords: a global "any completion label pending" test would send most indirect dispatches
    // to the CPU, as ~5 such labels are pending per frame. Stamp 0 matches every live entry.
    const auto labelPending = [&] {
        for (std::uint64_t dword = arguments; dword < arguments + 12; dword += 4) {
            if (recorder.PendingLabel(dword, 4, 0).has_value()) return true;
        }
        return false;
    };
    if (Graphics::StorageTexture::FlushPending(arguments, 12, nullptr, "indirect dispatch arguments")) {
        // As the flush hook does: the stores were only recorded and the CPU is about to read them.
        outcome.cpuReason = 1;
        Graphics::Recorder::CountSync(2);
        recorder.Sync();
    } else if (record.shader->program->workgroupMemoryDwords != 0) {
        outcome.cpuReason = 8;
    } else if (labelPending() || state->CopiedWriterOverlaps(arguments, 12)) {
        outcome.cpuReason = 2;
    } else if ((record.argumentImport = Graphics::HostImportFor(context, arguments, 12)) == nullptr || (threadDimensions && record.argumentImport->address == 0)) {
        record.argumentImport = nullptr;
        // Valid until the next refreshImports, which only runs under GuestMemory::GpuMutex (held
        // here, by every Dispatch caller) and Keeps a retired VkBuffer while the recorder is busy.
        outcome.cpuReason = 3;
    }
    if (timer.profile) {
        auto& hold = Dispatches().indirectHold;
        hold.writersScanned += state->copiedWriters->size();
        hold.drawWritersScanned += Graphics::DrawCopiedWriters()->size();
        hold.maxWriters = std::max(hold.maxWriters, state->copiedWriters->size());
    }
    timer.phase(PhaseDecide);
    if (outcome.cpuReason == 0) return;
    const auto readStart = std::chrono::steady_clock::now();
    Graphics::FinishLeaseWork();
    std::array<std::uint32_t, 3> groups{};
    GuestMemory::Read(arguments, std::as_writable_bytes(std::span(groups)), 4);
    if (threadDimensions) {
        record.threadLimit = groups;
        for (std::size_t axis = 0; axis < groups.size(); ++axis) groups[axis] = groups[axis] / record.threadGroup[axis] + (groups[axis] % record.threadGroup[axis] != 0 ? 1u : 0u);
    }
    outcome.argumentReadMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - readStart).count();
    record.x = groups[0];
    record.y = groups[1];
    record.z = groups[2];
    record.cpuArguments = arguments;
    record.cpuReason = outcome.cpuReason;
    // Timed before `arguments` is cleared, so the read counts as an indirect phase.
    timer.phase(PhaseArgumentRead);
    // From here on the phases of a CPU-resolved indirect dispatch are charged as direct ones.
    record.arguments = 0;
    timer.indirect = false;
    std::snprintf(groupsText, 40, "%ux%ux%u", record.x, record.y, record.z);
}

void VulkanDevice::recordDispatch(RecordedDispatch& record) {
    auto& d = Dispatches();
    auto& recorder = *state->recorder;
    auto& timer = *record.timer;
    const auto& context = *record.context;
    auto& resources = *record.resources;
    const auto arguments = record.arguments;
    const auto* argumentImport = record.argumentImport;
    const auto* limit = state->properties.limits.maxComputeWorkGroupCount;
    if (argumentImport == nullptr && record.shader->program->workgroupMemoryDwords != 0 && (record.x > limit[0] || record.y > limit[1] || record.z > limit[2])) {
        char counted[128] = "";
        if (record.cpuArguments != 0) std::snprintf(counted, sizeof(counted), " (read on the CPU from the indirect arguments at 0x%llx, cpuReason %d)", static_cast<unsigned long long>(record.cpuArguments), record.cpuReason);
        char text[384];
        std::snprintf(text, sizeof(text), "Vulkan dispatch: workgroup count %ux%ux%u%s exceeds device limits %ux%ux%u and a split aliases the device-memory LDS slots, whose base is computed from gl_NumWorkGroups of a single chunk", record.x, record.y, record.z, counted, limit[0], limit[1], limit[2]);
        throw std::runtime_error(text);
    }
    // The record phase split (APS5_PROFILE_DRAW, rows "record: ..." of the [dispatch] totals):
    // opening the batch, the keeps, the data refresh, the barriers with the bind and the dispatch
    // itself, the pending-write notes and marks (MarkGpuWrites), and the completion registration.
    auto recordFrom = timer.profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const auto recordStep = [&](DispatchPhase which) {
        if (!timer.profile) return;
        const auto now = std::chrono::steady_clock::now();
        timer.add(which, std::chrono::duration<double, std::milli>(now - recordFrom).count());
        recordFrom = now;
    };
    // Whether the last command recorded into the batch left every earlier write visible to this
    // dispatch's reads and writes (a closed label-store run, the previous dispatch's own trailing
    // barrier, a fill's or copy's): the pre-dispatch barrier below then adds nothing and is
    // skipped. APS5_NO_BARRIER_ELISION=1 or APS5_FULL_BARRIERS=1 records it every time.
    const bool barrierElision = Graphics::Recorder::MergeBarriers();
    // A queued DCC key store over memory this dispatch writes or reads in place (unknown for an
    // address-based build), or over its indirect arguments, must land before it: the title's clear
    // kernels store keys through V#s, its decompress kernels read them. A queued label store over
    // such memory likewise (the dispatch would read a stale dword, or its write would lose to
    // the label).
    const auto touches = [&](std::uint64_t begin, std::uint64_t end) {
        const auto bytes = static_cast<std::size_t>(end - begin);
        return resources.WritesOverlap(begin, bytes) || resources.ReadsOverlap(begin, bytes) || (argumentImport != nullptr && begin < arguments + 12 && arguments < end);
    };
    if (recorder.HasQueuedKeyStores() && (resources.HoldsLease() || recorder.AnyQueuedKeyStore(touches))) recorder.FlushKeyStores();
    if (recorder.HasQueuedStores() && (resources.HoldsLease() || recorder.AnyQueuedStore(touches))) recorder.FlushStores();
    VkAccessFlags covered = 0;
    const auto commands = recorder.Commands(&covered);
    recordStep(PhaseRecordCommands);
    recorder.Keep(record.objects);
    if (record.resources != nullptr) record.resources->KeepStorageImageBytes(recorder);
    recorder.Keep(record.resources);
    if (record.resources != nullptr) recorder.KeepBytes(record.resources.get(), record.resources->CopiedBytes());
    if (record.bindings != nullptr) {
        recorder.Keep(record.bindings);
        for (const auto& snapshot : record.bindings->snapshots) recorder.KeepBytes(snapshot.buffer.get(), snapshot.buffer->Bytes().size());
    }
    recordStep(PhaseRecordKeeps);
    if (record.dataRefresh != RecordedDispatch::DataRefresh::None) {
        // The template's data buffers take this dispatch's words: a transfer write the pre-dispatch
        // barrier makes visible (so it is recorded whatever the previous command covered), ordered
        // after an earlier dispatch's reads of the buffers by that dispatch's trailing barrier. A
        // recipe compares the two 64-bit hashes first: equal hashes mean the buffers hold the words.
        const bool differs = record.dataRefresh == RecordedDispatch::DataRefresh::Words || resources.DataWordsHash() != record.dataWordsHash;
        if (timer.profile && record.shader->program != nullptr && record.shader->program->bindings.size() != resources.BindingCount()) ++d.templateByNumber;
        if (differs && (record.resourceKeyMatched ? resources.refreshData(commands, *record.shader, &recorder) : resources.RefreshData(commands, *record.shader, &recorder))) {
            covered = 0;
            ++d.templateRefreshed;
            d.templateRevalidateMs += record.revalidateMs;
            record.refreshed = true;
        } else {
            ++d.templateSameWords;
        }
        recordStep(PhaseRecordDataRefresh);
    }
    std::shared_ptr<Graphics::Buffer> threadArguments;
    if (record.threadGroup != std::array<std::uint32_t, 3>{}) {
        const auto data = resources.shaderDataBuffer(*record.shader);
        const auto limitDword = record.shader->program->imageMetadataDword;
        Graphics::Require(data != VK_NULL_HANDLE && limitDword >= 3, "Vulkan dispatch: a thread-dimension dispatch has no ShaderData thread limit");
        const VkDeviceSize limitByte = static_cast<VkDeviceSize>(limitDword - 3) * sizeof(std::uint32_t);
        Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        if (argumentImport != nullptr) {
            threadArguments = std::make_shared<Graphics::Buffer>(context, 24, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            Graphics::Require(recorder.RecordDispatchArguments(commands, argumentImport->address + (arguments - argumentImport->base), threadArguments->DeviceAddress(), &record.threadGroup), "Vulkan dispatch: thread-dimension indirect arguments need the argument kernel");
            const VkBufferCopy limit{12, limitByte, 12};
            context.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(commands, threadArguments->Handle(), data, 1, &limit);
        } else {
            context.Resolved(&Graphics::DeviceFunctions::cmdUpdateBuffer, "vkCmdUpdateBuffer")(commands, data, limitByte, sizeof(record.threadLimit), record.threadLimit.data());
        }
        resources.forgetShaderDataWords(*record.shader);
        covered = 0;
    }
    using CommandClass = Graphics::Recorder::CommandClass;
    if (Graphics::Recorder::BarrierValidate()) {
        if (argumentImport != nullptr) {
            const std::pair<std::uint64_t, std::uint64_t> argumentRange{arguments, arguments + 12};
            recorder.NoteAccess(CommandClass::IndirectArguments, Graphics::Recorder::Access{std::span(&argumentRange, 1), {}, {}, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT});
        }
        const auto reads = resources.InPlaceReads();
        const auto images = resources.StorageImages();
        recorder.NoteAccess(CommandClass::DispatchLeading, Graphics::Recorder::Access{reads, resources.GpuWrites(), images, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, resources.HoldsLease()});
    }
    if (argumentImport != nullptr) {
        // The group counts were stored by earlier recorded work (a dispatch in place, a fill) or the
        // host; the indirect read follows all of it.
        const auto timing = recorder.BeginGpuTiming(CommandClass::IndirectArguments);
        Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
        Graphics::Recorder::CountBarriers(CommandClass::IndirectArguments);
        recorder.EndGpuTiming(timing, 12);
        recorder.NotePendingRead(arguments, 12, Graphics::Recorder::ReadKind::Indirect);
    }
    // Results of earlier recorded work are visible to this dispatch, its own to everything after.
    constexpr VkAccessFlags shaderAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    if ((covered & shaderAccess) == shaderAccess && barrierElision) {
        ++d.preBarriersSkipped;
        Graphics::Recorder::CountMerged(CommandClass::DispatchLeading);
    } else {
        const auto timing = recorder.BeginGpuTiming(CommandClass::DispatchLeading);
        Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        Graphics::Recorder::CountBarriers(CommandClass::DispatchLeading);
        recorder.EndGpuTiming(timing);
        ++d.preBarriersRecorded;
    }
    VkBuffer indirectBuffer = argumentImport != nullptr ? argumentImport->buffer : VK_NULL_HANDLE;
    VkDeviceSize indirectOffset = argumentImport != nullptr ? Graphics::HostImportOffset(*argumentImport, arguments) : 0;
    if (threadArguments != nullptr) {
        indirectBuffer = threadArguments->Handle();
        indirectOffset = 0;
        recorder.Keep(std::move(threadArguments));
    } else if (argumentImport != nullptr && argumentImport->address != 0) {
        auto checked = std::make_shared<Graphics::Buffer>(context, 12, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        if (recorder.RecordDispatchArguments(commands, argumentImport->address + (arguments - argumentImport->base), checked->DeviceAddress())) {
            indirectBuffer = checked->Handle();
            indirectOffset = 0;
            recorder.Keep(std::move(checked));
        }
    }
    context.Resolved(&Graphics::DeviceFunctions::cmdBindPipeline, "vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, record.objects->pipeline);
    if (record.bindings != nullptr) Graphics::Require(record.bindings->allocation.set != VK_NULL_HANDLE, "dispatch overlay has no descriptor set");
    if (record.bindings != nullptr) context.Resolved(&Graphics::DeviceFunctions::cmdBindDescriptorSets, "vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, record.objects->layout, 0, 1, &record.bindings->allocation.set, 0, nullptr);
    else resources.Bind(commands, VK_PIPELINE_BIND_POINT_COMPUTE, record.objects->layout);
    if (const auto dwords = record.shader->program->workgroupMemoryDwords; dwords != 0) {
        auto memory = state->WorkgroupMemoryFor(context, record.x, record.y, record.z, dwords);
        context.Resolved(&Graphics::DeviceFunctions::cmdBindDescriptorSets, "vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, record.objects->layout, ShaderRecompiler::WorkgroupMemoryDescriptorSet, 1, &memory->set, 0, nullptr);
        recorder.Keep(std::move(memory));
    }
    if (record.pushStages != 0) {
        context.Resolved(&Graphics::DeviceFunctions::cmdPushConstants, "vkCmdPushConstants")(commands, record.objects->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, Graphics::ComputePushConstantBytes, record.pushBytes->data());
    }
    const auto gpuTiming = recorder.BeginGpuTiming(record.programAddress != 0 ? record.programAddress : record.shader->program->variantId);
    if (state->checkpointMarker != nullptr) state->checkpointMarker(commands, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(record.programAddress)));
    const auto marker = state->bufferMarker != nullptr ? state->markerLog.Next(record.programAddress) : 0u;
    if (marker != 0) state->bufferMarker(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, state->markerBuffer, 0, marker);
    if (argumentImport != nullptr) {
        context.Resolved(&Graphics::DeviceFunctions::cmdDispatchIndirect, "vkCmdDispatchIndirect")(commands, indirectBuffer, indirectOffset);
    } else {
        if (record.x <= limit[0] && record.y <= limit[1] && record.z <= limit[2]) {
            context.Resolved(&Graphics::DeviceFunctions::cmdDispatch, "vkCmdDispatch")(commands, record.x, record.y, record.z);
        } else {
            char counted[128] = "";
            if (record.cpuArguments != 0) std::snprintf(counted, sizeof(counted), " (read on the CPU from the indirect arguments at 0x%llx, cpuReason %d)", static_cast<unsigned long long>(record.cpuArguments), record.cpuReason);
            const auto offsets = std::ranges::count_if(record.shader->program->specialization, [](const auto& constant) { return ShaderRecompiler::PipelineSpecialization::IsWorkgroupOffset(constant.id); });
            if (offsets != ShaderRecompiler::PipelineSpecialization::WorkgroupOffsetWords) {
                char text[320];
                std::snprintf(text, sizeof(text), "Vulkan dispatch: workgroup count %ux%ux%u%s exceeds device limits %ux%ux%u and the shader takes no workgroup offset", record.x, record.y, record.z, counted, limit[0], limit[1], limit[2]);
                throw std::runtime_error(text);
            }
            const auto bindPipeline = context.Resolved(&Graphics::DeviceFunctions::cmdBindPipeline, "vkCmdBindPipeline");
            const auto dispatchChunk = context.Resolved(&Graphics::DeviceFunctions::cmdDispatch, "vkCmdDispatch");
            bool firstChunk = true;
            for (std::uint32_t zBase = 0; zBase < record.z; zBase += limit[2]) {
                for (std::uint32_t yBase = 0; yBase < record.y; yBase += limit[1]) {
                    for (std::uint32_t xBase = 0; xBase < record.x; xBase += limit[0]) {
                        if (!firstChunk) {
                            Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
                            bindPipeline(commands, VK_PIPELINE_BIND_POINT_COMPUTE, splitComputePipeline(context, *record.shader->program, *record.objects, {xBase, yBase, zBase}));
                        }
                        firstChunk = false;
                        dispatchChunk(commands, std::min(limit[0], record.x - xBase), std::min(limit[1], record.y - yBase), std::min(limit[2], record.z - zBase));
                    }
                }
            }
        }
    }
    recorder.EndGpuTiming(gpuTiming);
    if (marker != 0) state->bufferMarker(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, state->markerBuffer, sizeof(std::uint32_t), marker);
    const auto trailingTiming = recorder.BeginGpuTiming(CommandClass::DispatchTrailing);
    constexpr VkAccessFlags dispatchedAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT;
    Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_SHADER_WRITE_BIT, dispatchedAccess);
    Graphics::Recorder::CountBarriers(CommandClass::DispatchTrailing);
    recorder.EndGpuTiming(trailingTiming);
    recorder.MarkCovered(dispatchedAccess);
    recordStep(PhaseRecordBind);
    if (record.htileClear.has_value()) Graphics::ArmHtileDepthClear(record.htileClear->first, record.htileClear->second);
    resources.MarkGpuWrites(recorder);
    if (record.htileClear.has_value()) {
        Graphics::QueueHtileDepthClear(record.htileClear->first, record.htileClear->second);
        recorder.NotePendingWrite(record.htileClear->first, record.htileClear->second);
    }
    recordStep(PhaseRecordMarks);
    // Only copied written buffers (and BDA fault checks) need work once the GPU is done; without them
    // the batch can signal its labels from the GPU.
    const bool completionNeeded = resources.NeedsCompletion() || record.htileClear.has_value();
    if (completionNeeded && resources.Reusable()) {
        auto kept = record.resources;
        recorder.OnComplete([kept, htileClear = record.htileClear] {
            kept->CheckFaults();
            if (htileClear.has_value()) Graphics::CompleteHtileDepthClear(htileClear->first, htileClear->second);
        });
    } else if (completionNeeded) {
        // Listed for DispatchIndirect until the write-back ran (a non-reusable object, so once).
        auto writers = state->copiedWriters;
        auto kept = record.resources;
        recorder.OnComplete([kept, writers, htileClear = record.htileClear] {
            // Delisted before the write-back: one that fails must not keep indirect dispatches on the CPU.
            writers->erase(std::remove(writers->begin(), writers->end(), kept), writers->end());
            kept->WriteBackBuffers();
            if (htileClear.has_value()) Graphics::CompleteHtileDepthClear(htileClear->first, htileClear->second);
        });
        // Listed after the registration: a throw there leaves nothing that would pin the CPU path forever.
        if (resources.HasCopiedWrites()) writers->push_back(kept);
    }
    recordStep(PhaseRecordCompletion);
}

// The module, pipeline layout and pipeline of a compute result over `setLayout` (the set layout
// its bindings describe): what a dispatch or its preparation builds at the first use of a module,
// and what the journal replay builds on its own threads before that use.
std::shared_ptr<ComputePipelineObjects> VulkanDevice::createComputePipeline(const Graphics::Context& context, const ShaderRecompiler::RecompileResult& shader, VkDescriptorSetLayout setLayout, std::span<const std::uint32_t> layoutKey, VkShaderStageFlags pushStages, std::uint64_t programAddress) {
    PerformanceTimer timing("Vulkan.ComputePipeline");
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    const auto start = std::chrono::steady_clock::now();
    auto objects = std::make_shared<ComputePipelineObjects>();
    objects->device = state->device;
    objects->destroyModule = state->DeviceFunction<PFN_vkDestroyShaderModule>("vkDestroyShaderModule");
    objects->destroyLayout = state->DeviceFunction<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout");
    objects->destroyPipeline = state->DeviceFunction<PFN_vkDestroyPipeline>("vkDestroyPipeline");
    objects->module = Graphics::CreateShaderModule(state->DeviceFunction<PFN_vkCreateShaderModule>("vkCreateShaderModule"), state->device, state->graphicsPipelineLibrary, shader.spirv.data(), shader.spirv.size() * sizeof(std::uint32_t), "vkCreateShaderModule");
    objects->resourcesLayout = setLayout;
    const std::array<VkDescriptorSetLayout, 2> setLayouts{setLayout, shader.workgroupMemoryDwords != 0 ? state->workgroupMemoryLayout : VK_NULL_HANDLE};
    const VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0, Graphics::ComputePushConstantBytes};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = shader.workgroupMemoryDwords != 0 ? ShaderRecompiler::WorkgroupMemoryDescriptorSet + 1 : 1;
    layoutInfo.pSetLayouts = setLayouts.data();
    layoutInfo.pushConstantRangeCount = pushStages != 0 ? 1 : 0;
    layoutInfo.pPushConstantRanges = pushStages != 0 ? &push : nullptr;
    check(state->DeviceFunction<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(state->device, &layoutInfo, nullptr, &objects->layout), "vkCreatePipelineLayout");
    const VkPipelineCreateFlags pipelineFlags = computePipelineFlags();
    const Graphics::PipelineSpecialization specialization(shader);
    const auto subgroupSize = computeSubgroupSize(shader);
    const auto stageFlags = computeStageFlags(shader);
    if (const auto* directory = ComputePipelineDumpDirectory(); directory != nullptr && *directory != 0 && !state->probingFloatControls && ComputePipelineCapture::OptionalEnabled() && CaptureComputePipeline(shader.spirv.size())) {
        ComputePipelineCapture capture;
        capture.spirv = shader.spirv.Copy();
        ComputeDescriptorSetLayout resourcesLayout;
        const bool updateAfterBind = layoutKey.size() % 4u == 1u;
        resourcesLayout.flags = updateAfterBind ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT : 0;
        for (std::size_t index = 0; index + 4u <= layoutKey.size(); index += 4u) {
            const auto type = static_cast<VkDescriptorType>(layoutKey[index + 1u]);
            resourcesLayout.bindings.push_back({layoutKey[index], type, layoutKey[index + 2u], layoutKey[index + 3u], updateAfterBind && type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ? VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT : 0u});
        }
        capture.sets.push_back(std::move(resourcesLayout));
        if (shader.workgroupMemoryDwords != 0) capture.sets.push_back({0, {{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, 0}}});
        if (pushStages != 0) capture.pushConstants.push_back(push);
        for (const auto& value : shader.specialization) capture.specialization.push_back({value.id, value.value});
        capture.requiredSubgroupSize = subgroupSize;
        capture.stageFlags = stageFlags;
        capture.flags = pipelineFlags;
        capture.deviceName = state->properties.deviceName;
        capture.vendorId = state->properties.vendorID;
        capture.deviceId = state->properties.deviceID;
        capture.driverVersion = state->properties.driverVersion;
        capture.apiVersion = state->properties.apiVersion;
        std::copy_n(state->properties.pipelineCacheUUID, VK_UUID_SIZE, capture.pipelineCacheUuid.begin());
        const auto path = capture.WriteOptional(directory);
        if (!path.empty()) std::fprintf(stderr, "[compute-capture] %s\n", path.string().c_str());
    }
    if (profile && shader.spirv.size() > 100000) std::fprintf(stderr, "[dispatch] creating a pipeline for %zu SPIR-V words (program 0x%llx)\n", shader.spirv.size(), static_cast<unsigned long long>(programAddress));
    CreateComputePipeline(state->device, state->DeviceFunction<PFN_vkCreateComputePipelines>("vkCreateComputePipelines"), context.pipelineCache, objects->module, objects->layout, pipelineFlags, stageFlags, specialization.Info(), subgroupSize, &objects->pipeline, context.pipelineCacheSynchronization);
    objects->destroyModule(objects->device, objects->module, nullptr);
    objects->module = VK_NULL_HANDLE;
    if (!state->probingFloatControls) {
        computePipelinesCreated.fetch_add(1, std::memory_order_relaxed);
        state->computePipelinesCreated.fetch_add(1, std::memory_order_relaxed);
        if (GuestMemory::GpuMutex().HeldByThisThread()) computePipelinesCreatedLocked.fetch_add(1, std::memory_order_relaxed);
    }
    Graphics::LogPipelineStatistics_nid_no_patch(context, objects->pipeline);
    if (profile && shader.spirv.size() > 100000) std::fprintf(stderr, "[dispatch] created a pipeline for %zu SPIR-V words in %.0f ms (program 0x%llx)\n", shader.spirv.size(), std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count(), static_cast<unsigned long long>(programAddress));
    return objects;
}

VkPipeline VulkanDevice::splitComputePipeline(const Graphics::Context& context, const ShaderRecompiler::RecompileResult& shader, ComputePipelineObjects& objects, const std::array<std::uint32_t, 3>& base) {
    std::lock_guard lock(objects.splitMutex);
    if (const auto found = objects.splitPipelines.find(base); found != objects.splitPipelines.end()) return found->second;
    std::array<ShaderRecompiler::PipelineSpecializationConstant, ShaderRecompiler::PipelineSpecialization::WorkgroupOffsetWords> offsets{};
    for (std::uint32_t component = 0; component < offsets.size(); ++component) offsets[component] = {ShaderRecompiler::PipelineSpecialization::WorkgroupOffsetBase + component, base[component]};
    const Graphics::PipelineSpecialization specialization(shader, offsets);
    VkShaderModule module = Graphics::CreateShaderModule(state->DeviceFunction<PFN_vkCreateShaderModule>("vkCreateShaderModule"), state->device, state->graphicsPipelineLibrary, shader.spirv.data(), shader.spirv.size() * sizeof(std::uint32_t), "vkCreateShaderModule");
    VkPipeline pipeline = VK_NULL_HANDLE;
    try {
        CreateComputePipeline(state->device, state->DeviceFunction<PFN_vkCreateComputePipelines>("vkCreateComputePipelines"), context.pipelineCache, module, objects.layout, computePipelineFlags(), computeStageFlags(shader), specialization.Info(), computeSubgroupSize(shader), &pipeline, context.pipelineCacheSynchronization);
    } catch (...) {
        objects.destroyModule(objects.device, module, nullptr);
        throw;
    }
    objects.destroyModule(objects.device, module, nullptr);
    computePipelinesCreated.fetch_add(1, std::memory_order_relaxed);
    state->computePipelinesCreated.fetch_add(1, std::memory_order_relaxed);
    if (GuestMemory::GpuMutex().HeldByThisThread()) computePipelinesCreatedLocked.fetch_add(1, std::memory_order_relaxed);
    objects.splitPipelines.emplace(base, pipeline);
    return pipeline;
}

std::shared_ptr<ComputePipelineObjects> VulkanDevice::computePipeline(const Graphics::Context& context, const ShaderRecompiler::RecompileResult& shader, VkDescriptorSetLayout setLayout, std::span<const std::uint32_t> layoutKey, VkShaderStageFlags pushStages, std::uint64_t programAddress) {
    const auto pipelineKey = ComputePipelineKey::Of(shader, pushStages != 0);
    std::promise<std::shared_ptr<ComputePipelineObjects>> building;
    if (!pipelineKey.identity.Empty()) {
        std::shared_future<std::shared_ptr<ComputePipelineObjects>> other;
        {
            std::lock_guard pipelines(state->computePipelinesMutex);
            if (const auto found = state->computePipelines.find(pipelineKey); found != state->computePipelines.end()) return found->second;
            if (const auto pending = state->computePipelinesBuilding.find(pipelineKey); pending != state->computePipelinesBuilding.end()) other = pending->second;
            else state->computePipelinesBuilding.emplace(pipelineKey, building.get_future().share());
        }
        if (other.valid()) return other.get();
    }
    const auto finish = [&](std::shared_ptr<ComputePipelineObjects> built, std::exception_ptr error) {
        if (pipelineKey.identity.Empty()) return built;
        {
            std::lock_guard pipelines(state->computePipelinesMutex);
            if (built != nullptr) built = state->computePipelines.try_emplace(pipelineKey, built).first->second;
            state->computePipelinesBuilding.erase(pipelineKey);
        }
        if (error != nullptr) building.set_exception(error);
        else building.set_value(built);
        return built;
    };
    std::shared_ptr<ComputePipelineObjects> objects;
    try {
        objects = createComputePipeline(context, shader, setLayout, layoutKey, pushStages, programAddress);
        if (Graphics::PipelineJournalEnabled() && !state->probingFloatControls) Graphics::RecordComputePipeline(shader, pushStages != 0);
    } catch (...) {
        finish(nullptr, std::current_exception());
        throw;
    }
    return finish(std::move(objects), nullptr);
}

VulkanDevice::IndirectOutcome VulkanDevice::dispatch(const ShaderRecompiler::RecompileResult& shader, std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint64_t arguments, std::span<const Graphics::GuestMemorySnapshot> snapshots, std::uint64_t programAddress, std::shared_ptr<PreparedDispatch> prepared, std::shared_ptr<const Recipe>* recipeOut, const std::array<std::uint32_t, 3>& threadGroup, std::optional<std::pair<std::uint64_t, std::size_t>> htileClear) {
    startJournalReplay();
    for (const auto& binding : shader.bindings) {
        if (binding.role != ShaderRecompiler::DescriptorRole::GuestBuffers) continue;
        for (std::size_t at = 0; at + 4u <= binding.guestDescriptor.size(); at += 4u) {
            const auto* words = binding.guestDescriptor.data() + at;
            const auto base = static_cast<std::uint64_t>(words[0]) | (static_cast<std::uint64_t>(words[1] & 0xffffu) << 32u);
            const auto stride = (words[1] >> 16u) & 0x3fffu;
            const auto records = static_cast<std::uint64_t>(words[2]);
            const auto bytes = stride != 0 ? records * stride : records;
            if (base != 0 && bytes != 0) static_cast<void>(Graphics::FlushDepthPlanes(base, static_cast<std::size_t>(bytes)));
        }
    }
    state->recorder->BoundKeptBytes();
    ShaderRecompiler::RuntimeAbi::RequireVersion(shader.runtimeAbiVersion);
    PerformanceTimer timing("Vulkan.Dispatch");
    if (recipeOut != nullptr) *recipeOut = nullptr;
    // Group counts for the trace lines; an indirect dispatch does not know them.
    char groupsText[40];
    if (arguments != 0) std::snprintf(groupsText, sizeof(groupsText), "indirect");
    else std::snprintf(groupsText, sizeof(groupsText), "%ux%ux%u", x, y, z);
    APS5_LOG_OUT_DEBUG("Dispatch groups=%s spirvWords=%zu bindings=%zu pushConstants=%zu", groupsText, shader.spirv.size(), shader.bindings.size(), shader.pushConstants.size());
    if (shader.spirv.size() < 5 || shader.spirv[0] != 0x07230203u) {
        throw std::runtime_error("Vulkan dispatch: invalid SPIR-V");
    }
    const std::array<Graphics::CompiledShader, 1> shaders{{{ShaderRecompiler::ShaderStage::Compute, &shader, 0}}};
    const auto pushStages = Graphics::PushConstantStages(shaders);
    if (pushStages != 0 && state->properties.limits.maxPushConstantsSize < Graphics::ComputePushConstantBytes) {
        throw std::runtime_error("Vulkan dispatch: compute push constant range exceeds device limit");
    }
    auto pushBytes = Graphics::AssemblePushConstants(shaders);
    const auto context = graphicsContext();
    const auto pipelineKey = ComputePipelineKey::Of(shader, pushStages != 0);
    std::shared_ptr<ComputePipelineObjects> objects = prepared != nullptr ? prepared->objects : nullptr;
    if (objects == nullptr && !pipelineKey.identity.Empty()) {
        std::lock_guard pipelines(state->computePipelinesMutex);
        if (const auto found = state->computePipelines.find(pipelineKey); found != state->computePipelines.end()) objects = found->second;
    }
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    auto& d = Dispatches();
    // This call's phases (indexed by DispatchPhase), formatted only for a slow call or a new
    // [indirect] maximum; the totals per phase across dispatches are reported every 1000 dispatches
    // under APS5_PROFILE_DRAW.
    DispatchTimer timer(profile, arguments != 0, shader.spirv.size(), programAddress);
    DispatchTimer::countDispatch(profile);
    const auto lookupsBefore = Graphics::DeviceProcLookups();
    std::shared_ptr<Graphics::ShaderResources> resources;
    ResourceCache::Key contentKey;
    const bool cacheable = ResourceCacheEnabled() && shader.PipelineVariantId() != 0;
    // The object came from the resource cache: a compute template whose data buffers take this
    // dispatch's words at the record (ShaderResources::RefreshData).
    bool fromCache = false;
    bool refusedMove = false;
    double revalidateMs = 0;
    std::shared_ptr<Graphics::ShaderResources::DrawBindings> overlay;
    // The batches the pre-sync (PrepareDispatch) waited for are reaped first: their completions
    // (CPU write-backs) land before stage B or a cached object's Revalidate reads, and the flush
    // hook then finds nothing pending over their surfaces.
    if (prepared != nullptr && prepared->presyncSerial != 0) {
        // Timed alone: the phase entry exists only when a reap ran, and measures just the reap.
        timer.restart();
        ReapRecorded(prepared->presyncSerial);
        timer.phase(PhaseReap);
    }
    // The 'resources' phase below, split (APS5_PROFILE_DRAW) into what runs under the mutex: the
    // sub-phases are extra rows named "resources: ..." in the [dispatch] totals and the [indirect]
    // line, so the hold's biggest part (stage B's image lookups, the whole build of an address-based
    // shader) is named, and "resources: hook waits" says how much of the build was the nested flush
    // hook waiting for recorded work (a locked wait: it overlaps the other rows).
    const auto subPhaseStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const auto waitedBefore = profile ? Graphics::Recorder::ThreadWaitedMs() : 0.0;
    // The image lookups of this call by outcome (stage B, a Revalidate or a locked build), for the
    // [indirect] line: reset here, read after the resources phase.
    if (profile && arguments != 0) Graphics::ThreadLookupOutcomes() = {};
    const auto noteRefusalBuild = [](bool inserted) {
        auto& moves = DispatchMoves();
        moves.rebuilt.fetch_add(1, std::memory_order_relaxed);
        if (inserted) moves.kept.fetch_add(1, std::memory_order_relaxed);
    };
    // The resource cache insert evicts the oldest entry, destroying its ShaderResources here when the
    // cache was the last owner: timed apart from the build.
    const auto insert = [&] {
        const auto insertStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        // The displaced entries go to the open batch: their destruction (descriptor sets, pooled
        // buffers, texture references) then runs on the release thread once the batch completed,
        // not here under the mutex. APS5_NO_DEFERRED_EVICTION=1 destroys them here as before.
        static const bool deferEviction = std::getenv("APS5_NO_DEFERRED_EVICTION") == nullptr;
        std::vector<std::shared_ptr<Graphics::ShaderResources>> evicted;
        state->resourceCache.Insert(contentKey, resources, deferEviction ? &evicted : nullptr);
        for (auto& object : evicted) {
            object->KeepStorageImageBytes(*state->recorder);
            state->recorder->Keep(std::move(object));
        }
        if (profile) {
            timer.add(PhaseResourcesInsert, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - insertStart).count());
            if (arguments != 0) d.indirectHold.phaseCounts[PhaseResourcesInsert] += evicted.size();
        }
    };
    if (prepared != nullptr && prepared->resources != nullptr) {
        // Stage A ran without the mutex (PrepareDispatch); stage B completes the build here.
        resources = std::move(prepared->resources);
        contentKey = std::move(prepared->key);
        // The build's own sub-phase totals before and after: the differences are stage B's parts
        // (an address-based build also runs its stage A here, under the mutex: 'A locked').
        const auto before = profile ? resources->Timing() : Graphics::ShaderResources::BuildTiming{};
        const auto completeStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        resources->Complete();
        require(prepared->describedLayoutKey.empty() || std::ranges::equal(resources->SetLayouts().mainKey, prepared->describedLayoutKey), "the set layout described for the dispatch's pipeline differs from its build");
        if (profile) {
            const auto completeWall = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - completeStart).count();
            timer.add(PhaseResourcesComplete, completeWall);
            const auto& after = resources->Timing();
            const auto images = after.bindingsMs - before.bindingsMs;
            const auto upload = after.uploadMs - before.uploadMs;
            const auto descriptors = after.descriptorsMs - before.descriptorsMs;
            const auto stageA = after.prepareMs - before.prepareMs;
            const auto stageB = after.completeMs - before.completeMs;
            timer.add(PhaseResourcesImages, images);
            timer.add(PhaseResourcesUpload, upload);
            timer.add(PhaseResourcesDescriptors, descriptors);
            timer.add(PhaseResourcesOther, std::max(0.0, stageB - images - upload - descriptors));
            timer.add(PhaseResourcesALocked, stageA);
            // What Complete() spends outside both stages' totals: an address-based build's lease
            // and mirror acquisition (prepareAddressBindings) before its locked stage A.
            timer.add(PhaseResourcesAddress, std::max(0.0, completeWall - stageA - stageB));
            timer.callMs[PhaseResourcesAUnlocked] += prepared->prepareMs;
            d.phaseTotals[PhaseResourcesAUnlocked] += prepared->prepareMs;
        }
        if (cacheable) {
            ++d.cacheMisses;
            if (prepared->exact) noteRefusalBuild(resources->Reusable());
            if (resources->Reusable()) insert();
        }
    } else if (cacheable) {
        // PrepareDispatch made the key already when it found the cached object, and carries the
        // object it found: revalidated as a map hit would be (an entry replaced meanwhile is left
        // alone by the pointer-conditional Remove). APS5_NO_PREPARED_FIND=1 looks the key up again.
        static const bool preparedFind = std::getenv("APS5_NO_PREPARED_FIND") == nullptr;
        const bool exactEntry = prepared != nullptr && prepared->exact;
        contentKey = prepared != nullptr && !prepared->key.empty() ? std::move(prepared->key) : DispatchContentKey(shaders[0], context.device);
        auto cached = preparedFind && prepared != nullptr && prepared->cached != nullptr ? std::move(prepared->cached) : state->resourceCache.Find(contentKey);
        if (cached != nullptr) {
            if (cached->Revalidate(shaders[0])) {
                bool usable = true;
                if (DispatchMove() && !exactEntry) {
                    auto& moves = DispatchMoves();
                    Graphics::ShaderResources::MoveProof proof;
                    auto moved = cached->MovedDispatchBuffers(shaders[0], state->recorder.get(), DispatchMoveBytes(), proof);
                    moves.checked.fetch_add(1, std::memory_order_relaxed);
                    if (moved.has_value() && threadGroup != std::array<std::uint32_t, 3>{}) {
                        const auto data = cached->shaderDataAllocation(shaders[0]);
                        if (data.has_value() && std::ranges::any_of(*moved, [&](const Graphics::ShaderResources::MovedBuffer& entry) { return entry.allocation == *data; })) moved.reset();
                    }
                    if (moved.has_value() && !moved->empty()) {
                        overlay = cached->PrepareDrawBindings(*state->recorder, *moved, true);
                        if (overlay != nullptr && overlay->allocation.set == VK_NULL_HANDLE) overlay.reset();
                        if (overlay == nullptr) {
                            proof.refusal = Graphics::ShaderResources::MoveRefusal::Bindings;
                            moved.reset();
                        }
                    }
                    if (!moved.has_value()) {
                        usable = false;
                        moves.refused[static_cast<std::size_t>(proof.refusal)].fetch_add(1, std::memory_order_relaxed);
                    } else if (moved->empty()) {
                        moves.exact.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        moves.moved.fetch_add(1, std::memory_order_relaxed);
                        moves.buffers.fetch_add(proof.buffers, std::memory_order_relaxed);
                        moves.bytes.fetch_add(proof.bytes, std::memory_order_relaxed);
                        noteDispatchMoveSize(proof.bytes);
                    }
                }
                if (!usable) {
                    contentKey = DispatchContentKey(shaders[0], context.device, false);
                    refusedMove = true;
                    cached = state->resourceCache.Find(contentKey);
                    if (cached != nullptr && cached->Revalidate(shaders[0])) usable = true;
                    else if (cached != nullptr) {
                        state->resourceCache.Remove(contentKey, cached.get());
                        ++d.cacheInvalidated;
                        state->recorder->Keep(std::move(cached));
                    }
                }
                if (usable) {
                    if (refusedMove || exactEntry) DispatchMoves().served.fetch_add(1, std::memory_order_relaxed);
                    resources = std::move(cached);
                    fromCache = true;
                    ++d.cacheHits;
                }
            } else {
                state->resourceCache.Remove(contentKey, cached.get());
                ++d.cacheInvalidated;
                // Off the mutex with its batch, as an evicted entry (insert()).
                cached->KeepStorageImageBytes(*state->recorder);
                state->recorder->Keep(std::move(cached));
            }
            if (profile) {
                revalidateMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - subPhaseStart).count();
                timer.add(PhaseResourcesRevalidate, revalidateMs);
            }
        }
    }
    if (resources == nullptr) {
        const auto buildStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        resources = std::make_shared<Graphics::ShaderResources>(context, shaders[0], snapshots, false, prepared != nullptr ? prepared->dispatchThreads : 0);
        if (profile) timer.add(PhaseResourcesFullBuild, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - buildStart).count());
        if (cacheable) {
            ++d.cacheMisses;
            if (refusedMove) noteRefusalBuild(resources->Reusable());
            if (resources->Reusable()) insert();
        }
    }
    resources->PatchPushConstants(pushBytes);
    if (profile) timer.add(PhaseResourcesHookWaits, Graphics::Recorder::ThreadWaitedMs() - waitedBefore);
    // The lookup outcomes of this call: rows with counts on the [indirect] line, and a summary for
    // the longest call.
    if (profile && arguments != 0) {
        const auto& outcomes = Graphics::ThreadLookupOutcomes();
        for (std::size_t kind = 0; kind < Graphics::LookupOutcomes::Count; ++kind) {
            if (outcomes.counts[kind] == 0) continue;
            const auto row = DispatchPhaseCount + kind;
            timer.callMs[row] += outcomes.ms[kind];
            d.indirectHold.phaseMs[row] += outcomes.ms[kind];
            d.indirectHold.phaseCounts[row] += outcomes.counts[kind];
        }
    }
    if (profile && std::chrono::steady_clock::now() - d.cacheReport > std::chrono::seconds(10)) {
        d.cacheReport = std::chrono::steady_clock::now();
        AgcDriver::ProfilePrint_nid_no_patch("[rescache] (cumulative): %llu hits, %llu misses, %llu invalidated, %zu entries (dispatch + draw); template hits: %llu refreshed the data buffers (revalidate %.1f ms), %llu had the same words, %llu paired their data bindings by number (no image table binding of their own); Find calls %llu, Touch calls %llu\n", static_cast<unsigned long long>(d.cacheHits), static_cast<unsigned long long>(d.cacheMisses), static_cast<unsigned long long>(d.cacheInvalidated), state->resourceCache.Size(), static_cast<unsigned long long>(d.templateRefreshed), d.templateRevalidateMs, static_cast<unsigned long long>(d.templateSameWords), static_cast<unsigned long long>(d.templateByNumber), static_cast<unsigned long long>(ResourceCache::Finds()), static_cast<unsigned long long>(ResourceCache::Touches()));
        AgcDriver::ProfilePrint_nid_no_patch("[vk] deviceProc lookups inside the device call (cumulative): %llu (%.2f per dispatch); pre-dispatch barriers recorded %llu, skipped %llu; compute pipelines created %llu (%llu under the GPU mutex)\n", static_cast<unsigned long long>(d.procLookups), d.profiledDispatches != 0 ? static_cast<double>(d.procLookups) / static_cast<double>(d.profiledDispatches) : 0.0, static_cast<unsigned long long>(d.preBarriersRecorded), static_cast<unsigned long long>(d.preBarriersSkipped), static_cast<unsigned long long>(computePipelinesCreated.load(std::memory_order_relaxed)), static_cast<unsigned long long>(computePipelinesCreatedLocked.load(std::memory_order_relaxed)));
        reportRecipes();
        reportDispatchMove();
    }
    timing.Mark("shader_resources");
    timer.phase(PhaseResources);
    static const bool verifySetLayouts = std::getenv("APS5_VERIFY_SET_LAYOUTS") != nullptr;
    if (verifySetLayouts && objects != nullptr) require(objects->resourcesLayout == resources->Layout(), "the resident compute pipeline's set layout differs from the dispatch's");
    if (objects == nullptr) {
        if (verifySetLayouts && context.descriptorCache != nullptr) {
            const auto described = Graphics::DescribeSetLayouts(context, shaders);
            require(described.layouts.main == resources->Layout() && !described.layouts.split, "the set layout described from the compute shader differs from the dispatch's");
        }
        const auto layouts = resources->SetLayouts();
        objects = computePipeline(context, shader, layouts.main, layouts.mainKey, pushStages, programAddress);
        timing.Mark("pipeline_create");
        timer.phase(PhasePipeline);
    }
    DispatchCompletion completion{context, shaders, pushStages, pushBytes, objects, resources, contentKey, cacheable, fromCache, revalidateMs, overlay, lookupsBefore, timer, timing, groupsText};
    return recordCompletedDispatch(shader, x, y, z, arguments, programAddress, recipeOut, threadGroup, htileClear, completion);
}

VulkanDevice::IndirectOutcome VulkanDevice::recordCompletedDispatch(const ShaderRecompiler::RecompileResult& shader, std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint64_t arguments, std::uint64_t programAddress, std::shared_ptr<const Recipe>* recipeOut, const std::array<std::uint32_t, 3>& threadGroup, std::optional<std::pair<std::uint64_t, std::size_t>> htileClear, DispatchCompletion& completion) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    const auto& context = completion.context;
    const auto& shaders = completion.shaders;
    const auto pushStages = completion.pushStages;
    const auto& pushBytes = completion.pushBytes;
    const auto& objects = completion.objects;
    const auto& resources = completion.resources;
    const auto& contentKey = completion.contentKey;
    const bool cacheable = completion.cacheable;
    const bool fromCache = completion.fromCache;
    const double revalidateMs = completion.revalidateMs;
    const auto& overlay = completion.overlay;
    const auto lookupsBefore = completion.lookupsBefore;
    auto& timer = completion.timer;
    auto& timing = completion.timing;
    auto* groupsText = completion.groupsText;
    // Recorded into the device's open batch: the CPU moves on to the next command while the GPU
    // works; the batch is waited for where the guest expects results (labels, flips, CPU reads).
    auto& recorder = *state->recorder;
    IndirectOutcome outcome{0, 0};
    RecordedDispatch record{&context, &shaders[0], resources, objects, pushStages, &pushBytes, x, y, z, arguments, nullptr, programAddress, fromCache && TemplateDataRefresh() ? RecordedDispatch::DataRefresh::Words : RecordedDispatch::DataRefresh::None, 0, revalidateMs, &timer, htileClear};
    record.resourceKeyMatched = fromCache;
    record.bindings = overlay;
    record.threadGroup = threadGroup;
    if (arguments != 0) decideIndirect(record, outcome, groupsText);
    // The indirect hold total below still covers the whole call of a CPU-resolved one.
    const bool indirect = record.arguments != 0 || outcome.cpuReason != 0;
    recordDispatch(record);
    timing.Mark("command_record");
    timer.phase(PhaseRecord);
    APS5_LOG_OUT_DEBUG("Dispatch recorded groups=%s", groupsText);
    // Address-based shaders pin guest allocations until their write-back, which the completion runs
    // when the batch finished (deferred lease release, see Graphics::SyncLeaseWork): a guest thread
    // that needs a leased allocation syncs the recorder itself through the registry's pin waiter.
    // APS5_SYNC_LEASE_DISPATCH=1 completes such dispatches at once, as before.
    if (SyncEachDispatch() || (resources->HoldsLease() && Graphics::SyncLeaseWork())) {
        Graphics::Recorder::CountSync(3);
        const auto syncStart = std::chrono::steady_clock::now();
        recorder.Sync();
        timer.phase(PhaseSync);
        if (resources->HoldsLease()) Graphics::CountLeaseOutcome(true, 0);
        if (profile) {
            // APS5_PROFILE_DRAW: the [recorder] line's "address-based" syncs by program, every 10 s,
            // so the programs still waiting here are known (under the GpuMutex, like the rest).
            struct Waits { std::uint64_t count = 0; double ms = 0; };
            static std::map<std::uint64_t, Waits> byProgram;
            static std::uint64_t syncs = 0;
            static double waitedMs = 0;
            static auto lastReport = std::chrono::steady_clock::now();
            const auto now = std::chrono::steady_clock::now();
            const auto ms = std::chrono::duration<double, std::milli>(now - syncStart).count();
            auto& waits = byProgram[programAddress != 0 ? programAddress : shader.PipelineVariantId()];
            ++waits.count;
            waits.ms += ms;
            ++syncs;
            waitedMs += ms;
            if (now - lastReport > std::chrono::seconds(10)) {
                lastReport = now;
                std::vector<std::pair<std::uint64_t, Waits>> hot(byProgram.begin(), byProgram.end());
                std::sort(hot.begin(), hot.end(), [](const auto& a, const auto& b) { return a.second.ms > b.second.ms; });
                AgcDriver::ProfilePrint_nid_no_patch("[address-sync] %llu address-based dispatch syncs waited %.1f s in total; by program (10 s):", static_cast<unsigned long long>(syncs), waitedMs / 1000);
                for (std::size_t i = 0; i < hot.size() && i < 8; ++i) AgcDriver::ProfilePrint_nid_no_patch(" 0x%llx x%llu %.0fms", static_cast<unsigned long long>(hot[i].first), static_cast<unsigned long long>(hot[i].second.count), hot[i].second.ms);
                AgcDriver::ProfilePrint_nid_no_patch("\n");
                byProgram.clear();
            }
        }
    } else if (resources->HoldsLease()) {
        // The lease is in the open batch (kept above, nothing submitted since): the pin waiter
        // finishes the recorder up to that batch's serial. Prints the [address-sync] leases line.
        Graphics::CountLeaseOutcome(false, recorder.Submissions() + 1);
    }
    if (TraceDispatchIo()) std::fprintf(stderr, "[dispatch-io] %s:%s\n", groupsText, resources->Describe().c_str());
    WatchMemory(programAddress);
    // The recipe for the caller's dispatch-cache variant (design_cpu_final M4, rule R3): only an
    // object the resource cache serves under this content key (reusable: no lease, no copied
    // writes, every direct region import- or mirror-served), so a hit's proof is the template's
    // Revalidate and nothing needs completion work.
    if (recipeOut != nullptr && DispatchRecipes() && cacheable && overlay == nullptr && resources->Reusable() && !resources->NeedsCompletion() && objects != nullptr && !contentKey.empty()) {
        auto recipe = std::make_shared<Recipe>();
        recipe->device = state->device;
        recipe->templateRef = resources;
        recipe->key = contentKey;
        recipe->objects = objects;
        recipe->pushBytes = pushBytes;
        recipe->pushes = pushStages != 0;
        recipe->dataWordsHash = Graphics::ShaderResources::DataWordsHash(shaders[0]);
        if (!NoPresync() && CanWaitUnlocked()) {
            recipe->presyncSurfaces = resources->PresyncSurfaces();
            recipe->presyncProof = Graphics::HostImportsIdentity(context);
        }
        recipe->needsCompletion = resources->NeedsCompletion();
        recipe->holdsLease = resources->HoldsLease();
        *recipeOut = std::move(recipe);
    }
    timer.finish(lookupsBefore, groupsText, indirect);
    return outcome;
}

RecipeOutcome VulkanDevice::DispatchRecipe(const ShaderRecompiler::RecompileResult& shader, std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint64_t arguments, std::uint64_t programAddress, const std::shared_ptr<RecipeHit>& hit, IndirectOutcome& outcome, const std::shared_ptr<PreparedDispatch>& verify, bool refreshByWords, const std::array<std::uint32_t, 3>& threadGroup) {
    state->recorder->BoundKeptBytes();
    ShaderRecompiler::RuntimeAbi::RequireVersion(shader.runtimeAbiVersion);
    PerformanceTimer timing("Vulkan.DispatchRecipe");
    outcome = {0, 0};
    char groupsText[40];
    if (arguments != 0) std::snprintf(groupsText, sizeof(groupsText), "indirect");
    else std::snprintf(groupsText, sizeof(groupsText), "%ux%ux%u", x, y, z);
    const auto& recipe = *hit->recipe;
    auto& counters = Recipes().kinds[hit->indirect ? 1 : 0];
    auto& d = Dispatches();
    const std::array<Graphics::CompiledShader, 1> shaders{{{ShaderRecompiler::ShaderStage::Compute, &shader, 0}}};
    const auto context = graphicsContext();
    if (recipe.needsCompletion || recipe.holdsLease || !hit->resources->Reusable()) throw std::runtime_error("Vulkan dispatch: recipe over a non-reusable template");
    // Without the refresh the template holds the words it was keyed with: a data-only hit takes
    // the ordinary path, whose word-keyed lookup serves or builds the object for the live words.
    if (refreshByWords && !TemplateDataRefresh()) return RecipeOutcome::Rebuild;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    DispatchTimer timer(profile, arguments != 0, shader.spirv.size(), programAddress);
    DispatchTimer::countDispatch(profile);
    const auto lookupsBefore = Graphics::DeviceProcLookups();
    // R10: the recipe names the device it was built on (the pre-check read it too, but the driver
    // may replace the device between the two).
    if (recipe.device != state->device) {
        counters.rebuildDevice.fetch_add(1, std::memory_order_relaxed);
        return RecipeOutcome::Rebuild;
    }
    if (hit->presyncSerial != 0) {
        timer.restart();
        ReapRecorded(hit->presyncSerial);
        timer.phase(PhaseReap);
    }
    if (verify != nullptr) {
        // APS5_VERIFY_RECIPE=1: the ordinary path's answers beside the recipe's. The found object
        // is the cache's under the content key (PrepareDispatch's Find), the pipeline objects the
        // map's, and the data refresh decision the per-word compare RefreshData makes; the proof
        // itself is the same Revalidate on the same object, so it needs no second run.
        // A found object that is not the recipe's template is no disagreement: the cache no longer
        // serves the template under its key (removed by a sibling variant's failed proof, evicted,
        // replaced by another worker's Insert) while the batch keeps it alive, so the ordinary path
        // would use a content-equal object and the comparison has no counterpart. Counted, and the
        // ordinary path runs with the prepared find (C10: the template goes to the batch).
        if (verify->cached.get() != hit->resources.get()) {
            counters.verifyReplaced.fetch_add(1, std::memory_order_relaxed);
            hit->resources->KeepStorageImageBytes(*state->recorder);
            state->recorder->Keep(std::move(hit->resources));
            return RecipeOutcome::Rebuild;
        }
        std::shared_ptr<ComputePipelineObjects> mapped;
        const auto pipelineKey = ComputePipelineKey::Of(shader, recipe.pushes);
        {
            std::lock_guard pipelines(state->computePipelinesMutex);
            if (const auto found = state->computePipelines.find(pipelineKey); found != state->computePipelines.end()) mapped = found->second;
        }
        // A data-only hit's words are the shader's, not the recipe's: its hash decision is
        // computed from the shader, as the record below decides by words.
        const bool byHash = TemplateDataRefresh() && hit->resources->DataWordsHash() != (refreshByWords ? Graphics::ShaderResources::DataWordsHash(shaders[0]) : recipe.dataWordsHash);
        const bool byWords = TemplateDataRefresh() && hit->resources->DataWordsDiffer(shaders[0]);
        const char* disagreement = mapped != hit->objects ? "the pipeline objects" : byHash != byWords ? "the RefreshData decision" : nullptr;
        if (disagreement != nullptr) {
            std::fprintf(stderr, "[recipe] APS5_VERIFY_RECIPE: %s of the recipe disagrees with the ordinary path (program 0x%llx)\n", disagreement, static_cast<unsigned long long>(programAddress));
            std::fflush(stderr);
            std::abort();
        }
        counters.verified.fetch_add(1, std::memory_order_relaxed);
    }
    // The proof (rules R6/R7): the template's Revalidate, T1 included; a failure removes the
    // template from the cache (the batch keeps it) and the caller rebuilds.
    const auto waitedBefore = profile ? Graphics::Recorder::ThreadWaitedMs() : 0.0;
    const auto proofStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    Graphics::ShaderResources::ProofReport report;
    const bool proved = hit->resources->ProveCurrent(shaders[0], &report);
    if (profile) {
        counters.proofNs.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - proofStart).count()), std::memory_order_relaxed);
        timer.add(PhaseResourcesHookWaits, Graphics::Recorder::ThreadWaitedMs() - waitedBefore);
    }
    timer.phase(PhaseProof);
    if (!proved) {
        state->resourceCache.Remove(recipe.key, hit->resources.get());
        ++d.cacheInvalidated;
        // Off the mutex with its batch, as an evicted entry.
        hit->resources->KeepStorageImageBytes(*state->recorder);
        state->recorder->Keep(std::move(hit->resources));
        counters.rebuilds[static_cast<std::size_t>(report.failure)].fetch_add(1, std::memory_order_relaxed);
        return RecipeOutcome::Rebuild;
    }
    counters.proofPaths[static_cast<std::size_t>(report.path)].fetch_add(1, std::memory_order_relaxed);
    ++d.cacheHits;
    state->resourceCache.Touch(recipe.key);
    // The recipe's hash names the words it was attached with, not a data-only hit's: that hit
    // refreshes by the per-word compare (RefreshData keeps the template's own hash exact, so a
    // later exact hit of a sibling variant compares correctly against it).
    const auto dataRefresh = !TemplateDataRefresh() ? RecordedDispatch::DataRefresh::None : refreshByWords ? RecordedDispatch::DataRefresh::Words : RecordedDispatch::DataRefresh::Hash;
    if (dataRefresh == RecordedDispatch::DataRefresh::Words) counters.refreshByWords.fetch_add(1, std::memory_order_relaxed);
    RecordedDispatch record{&context, &shaders[0], hit->resources, hit->objects, recipe.pushes ? VkShaderStageFlags{VK_SHADER_STAGE_COMPUTE_BIT} : VkShaderStageFlags{0}, &recipe.pushBytes, x, y, z, arguments, nullptr, programAddress, dataRefresh, recipe.dataWordsHash, 0, &timer};
    record.resourceKeyMatched = verify != nullptr;
    record.threadGroup = threadGroup;
    if (arguments != 0) decideIndirect(record, outcome, groupsText);
    const bool indirect = record.arguments != 0 || outcome.cpuReason != 0;
    const auto recordStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    recordDispatch(record);
    if (profile) counters.recordNs.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - recordStart).count()), std::memory_order_relaxed);
    timing.Mark("command_record");
    timer.phase(PhaseRecord);
    counters.hits.fetch_add(1, std::memory_order_relaxed);
    if (record.dataRefresh != RecordedDispatch::DataRefresh::None) (record.refreshed ? counters.dataRefreshed : counters.dataSkipped).fetch_add(1, std::memory_order_relaxed);
    APS5_LOG_OUT_DEBUG("Dispatch recorded from recipe groups=%s", groupsText);
    if (SyncEachDispatch()) {
        Graphics::Recorder::CountSync(3);
        state->recorder->Sync();
        timer.phase(PhaseSync);
    }
    if (TraceDispatchIo()) std::fprintf(stderr, "[dispatch-io] %s:%s\n", groupsText, hit->resources->Describe().c_str());
    WatchMemory(programAddress);
    timer.finish(lookupsBefore, groupsText, indirect);
    if (profile) reportRecipes();
    return RecipeOutcome::Recorded;
}

}
