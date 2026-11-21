#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libc/include/HostThread.hpp"
#include "prx/libSceAgcDriver/Execution/include/FrameDumpIo.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineSpecialization.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libSceAgcDriver/Execution/include/CompileBudget.hpp"
#include "BdaAbi.hpp"
#if ANYPS5_ENABLE_SPIRV_TOOLS
#include "SpirvBackend/SpirvOptimizer.hpp"
#endif
#include "prx/libSceAgcDriver/Graphics/include/PipelineJournal.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DeferredDestruction.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Execution/include/Float16ConversionProbe.hpp"
#include "prx/libSceAgcDriver/Execution/include/Float16NanProbe.hpp"
#include "prx/libSceAgcDriver/Execution/include/FloatFlushProbe.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/BdaFeatures.hpp"
#include "prx/libSceAgcDriver/Execution/include/HostReportSources.hpp"
#include "prx/libSceAgcDriver/Execution/include/HostReportVulkan.hpp"
#include "prx/libSceAgcDriver/Execution/include/BdaAtomics.hpp"
#include "prx/libSceAgcDriver/Execution/include/RasterOrderGroup.hpp"
#include "prx/libSceAgcDriver/Execution/include/DeviceFault.hpp"
#include "prx/libSceAgcDriver/Execution/include/FragmentBarycentric.hpp"
#include "prx/libSceAgcDriver/Execution/include/Float32FlushControl.hpp"
#include "prx/libSceAgcDriver/Execution/include/PipelineLinkOptimization.hpp"
#include "prx/libSceAgcDriver/Execution/include/DrawInputsInPlace.hpp"
#include "prx/libSceAgcDriver/Execution/include/SubgroupBarrier.hpp"
#include "prx/libSceAgcDriver/Execution/include/TessellationPipelineCache.hpp"
#include "prx/libSceAgcDriver/Execution/include/IndexedIndirectPatches.hpp"
#include "prx/libSceAgcDriver/Execution/include/SubgroupClock.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderDeviceProfile.hpp"
#include "prx/libSceAgcDriver/Execution/include/FrameLimit.hpp"
#include "prx/libSceAgcDriver/Execution/include/PresentMode.hpp"
#include "prx/libSceAgcDriver/Execution/include/SwapchainImages.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceOverlay.hpp"
#include "prx/libSceAgcDriver/Execution/include/KeyboardOverlay.hpp"
#include "prx/libSceAgcDriver/Execution/include/PadStatusOverlay.hpp"
#include "prx/libSceAgcDriver/Execution/include/PresentationPass.hpp"
#include "prx/libSceAgcDriver/Execution/include/PresentationScaler.hpp"
#include "prx/libSceAgcDriver/Execution/include/PresentationUpscaler.hpp"
#include "prx/libSceAgcDriver/Execution/include/Screenshot.hpp"
#include "prx/libSceAgcDriver/Execution/include/SubmissionSnapshots.hpp"
#include "prx/libSceAgcDriver/TraceRevision.hpp"
#include "prx/libSceAgcDriver/Execution/include/UpscaleMode.hpp"
#include "prx/libkernel/AppMetadata/include/AppMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PlaceholderImages.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Execution/include/DisplayFormat.hpp"
#include "prx/libSceAgcDriver/Execution/include/HostHdr.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SampleOrder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/StagedMerge.hpp"
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
#include "prx/libc/include/RunReport.hpp"
#include "prx/libc/include/RunReportComponent.hpp"
#include "RdnaDecoder/DiagnosticSink.hpp"
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
extern char** environ;
#endif
#include <atomic>
#include <chrono>
#include <charconv>
#include <condition_variable>
#include <future>
#include <fstream>
#include <cctype>
#include <cstdio>
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

namespace AgcDriver::Graphics {
std::uint32_t LastCheckpointMarkerSerial() noexcept;
std::uint32_t LastCheckpointMarkerToken() noexcept;
void TeardownCheckpointMarkers() noexcept;
void ResetCheckpointSnapshots() noexcept;
}
#include <limits>
#include <list>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>
#include <utility>

#ifndef _WIN32
extern char** environ;
#endif

namespace AgcDriver {

std::string SoftwareFallbackWarning(VkPhysicalDeviceType selectedType, std::string_view selectedName, std::string_view hardwareName, PresentationRejection rejection, bool requested) {
    if (requested || selectedType != VK_PHYSICAL_DEVICE_TYPE_CPU || hardwareName.empty() || rejection == PresentationRejection::None) return {};
    const auto reason = rejection == PresentationRejection::MissingSwapchain ? "does not support VK_KHR_swapchain" : "cannot present to this window with a graphics and compute queue";
    return "Vulkan warning: selected software device " + std::string(selectedName) + " because hardware GPU " + std::string(hardwareName) + " " + reason + "; performance may be poor";
}

DeviceQueuePlan PlanDeviceQueues(std::span<const VkQueueFamilyProperties> families, std::uint32_t family, bool settles, bool sparseBinding, bool presents) {
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
    if (presents && family < count) {
        if (const auto index = SelectPresentQueue(families[family].queueCount, plan.mainCount)) {
            plan.present = true;
            plan.presentIndex = *index;
            plan.mainCount = *index + 1;
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

bool ChooseDeviceFaultVendorBinary(bool requested, bool faultSupported, bool vendorBinarySupported) noexcept {
    return requested && faultSupported && vendorBinarySupported;
}

const char* DeviceFaultVendorDumpPath() noexcept {
    const char* path = std::getenv("APS5_DUMP_DEVICE_FAULT");
    return path != nullptr && *path != '\0' ? path : nullptr;
}

bool WriteDeviceFaultVendorBinary(const char* path, const void* data, std::size_t size) noexcept {
    try {
        if (path == nullptr || *path == '\0' || (data == nullptr && size != 0)) return false;
        std::ofstream dump(path, std::ios::binary | std::ios::trunc);
        if (!dump) return false;
        if (size != 0) {
            dump.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
            if (!dump) return false;
        }
        dump.close();
        return static_cast<bool>(dump);
    } catch (...) {
        return false;
    }
}

void DumpDeviceFaultVendorBinary(VkDevice device, PFN_vkGetDeviceFaultInfoEXT faultInfo, const char* path) noexcept {
    try {
        constexpr VkDeviceSize DeviceFaultVendorBinaryLimit = 64u * 1024u * 1024u;
        if (device == VK_NULL_HANDLE || faultInfo == nullptr || path == nullptr || *path == '\0') return;
        VkDeviceFaultCountsEXT counts{VK_STRUCTURE_TYPE_DEVICE_FAULT_COUNTS_EXT};
        if (faultInfo(device, &counts, nullptr) != VK_SUCCESS) return;
        if (counts.vendorBinarySize == 0) return;
        if (counts.vendorBinarySize > DeviceFaultVendorBinaryLimit) {
            std::fprintf(stderr, "[device-fault] vendor binary of %llu bytes exceeds the 64 MiB dump bound\n", static_cast<unsigned long long>(counts.vendorBinarySize));
            return;
        }
        std::vector<std::byte> binary(static_cast<std::size_t>(counts.vendorBinarySize));
        VkDeviceFaultInfoEXT info{VK_STRUCTURE_TYPE_DEVICE_FAULT_INFO_EXT};
        info.pVendorBinaryData = binary.data();
        counts.addressInfoCount = 0;
        counts.vendorInfoCount = 0;
        counts.vendorBinarySize = static_cast<VkDeviceSize>(binary.size());
        const auto result = faultInfo(device, &counts, &info);
        if (result != VK_SUCCESS && result != VK_INCOMPLETE) return;
        const auto bytes = static_cast<std::size_t>(std::min<VkDeviceSize>(counts.vendorBinarySize, binary.size()));
        if (!WriteDeviceFaultVendorBinary(path, binary.data(), bytes)) {
            std::fprintf(stderr, "[device-fault] vendor binary dump to %s failed\n", path);
            return;
        }
        std::fprintf(stderr, "[device-fault] vendor binary: %zu bytes written to %s\n", bytes, path);
    } catch (...) {
    }
}

APS5_RUN_REPORT_COMPONENT("libSceAgcDriver")

namespace {

std::atomic<unsigned long long> descriptorUse[7 * 4];

void CountDescriptor(int type, int, int usage) {
    if (!LibcRunReportEnabled_nid_no_patch()) return;
    if (type >= 0 && type < 7 && usage >= 0 && usage < 4) descriptorUse[type * 4 + usage].fetch_add(1, std::memory_order_relaxed);
}

const ShaderRecompiler::DiagnosticSink runReportSink{
    .enabled = LibcRunReportEnabled_nid_no_patch,
    .shader = LibcRunReportShader_nid_no_patch,
    .unsupportedInstruction = LibcRunReportUnsupportedInstruction_nid_no_patch,
    .imageFormat = LibcRunReportImageFormat_nid_no_patch,
    .descriptor = CountDescriptor,
    .compileTime = LibcRunReportShaderCompileTime_nid_no_patch,
};
[[maybe_unused]] const bool runReportSinkInstalled = (LibcRunReportSetDescriptorCounters_nid_no_patch(descriptorUse), ShaderRecompiler::SetDiagnosticSink(&runReportSink), true);


std::uint64_t fillCompletionSyncs = 0;

void check(VkResult result, const char* operation) {
    if (result == VK_ERROR_DEVICE_LOST) Graphics::ReportDeviceLost();
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string(operation) + ": Vulkan result " + std::to_string(result));
    }
}

void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(std::string("Vulkan presentation: ") + reason);
}

void checkHostRequirements(const HostGpuQuery& query, bool printReport) {
    HostGpuFacts gpu;
    try {
        gpu = CollectHostGpuFacts(query);
    } catch (const std::exception& error) {
        if (printReport) std::fprintf(stderr, "AnyPS5 host report: the GPU could not be queried: %s\n", error.what());
        return;
    }
    const bool lacking = !MissingRequirements(gpu).empty();
    if (!printReport && !lacking) return;
    auto facts = printReport ? CollectHostFacts(SystemHostSources()) : HostFacts{};
    if (!printReport) {
        try {
            facts.redactions = SystemHostSources().redactions();
        } catch (const std::exception&) {
        }
    }
    if (printReport) {
        static std::atomic<bool> printed{false};
        if (!printed.exchange(true)) {
            facts.gpu = gpu;
            const auto text = FormatHostReport(facts);
            std::fputs(text.c_str(), stderr);
            std::fflush(stderr);
        }
    }
    if (lacking) throw std::runtime_error(DescribeMissingRequirements(gpu, facts.redactions));
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

using ResourceCache = Graphics::ResourceCache;

std::uint32_t DumpScale();
void WriteFrameBmp(int index, std::uint32_t width, std::uint32_t height, std::span<const std::byte> pixels);
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

    void Enqueue(int index, std::uint32_t width, std::uint32_t height, bool screenshot, std::vector<std::byte> pixels) {
        std::unique_lock lock(mutex);
        require(!stopping, "frame dump writer has stopped");
        if (!screenshot && !frameDumpOutput.Enabled_nid_no_patch()) return;
        require(pixels.size() <= maxBytes, "a frame dump exceeds the 64 MiB queue");
        changed.wait(lock, [&] { return stopping || pixels.size() <= maxBytes - queuedBytes; });
        require(!stopping, "frame dump writer has stopped");
        if (!worker.joinable()) worker = Libc::StartHostThread([this] { run(); });
        const auto bytes = pixels.size();
        pending.push_back({index, width, height, screenshot, std::move(pixels)});
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
                    else WriteFrameBmp(frame.index, frame.width, frame.height, frame.pixels);
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

VulkanDevice::PresentStatistics presentCounters{};
std::atomic<std::uint64_t> upscaledPresents{0};

}

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

struct ComputePipelineKey {
    ShaderRecompiler::PipelineIdentity identity;
    bool pushConstants = false;
    auto operator<=>(const ComputePipelineKey&) const = default;
    static ComputePipelineKey Of(const ShaderRecompiler::RecompileResult& shader, bool pushConstants) { return {Graphics::DescribedPipelineIdentity(shader), pushConstants}; }
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

struct ScratchPool {
    VkDevice device = VK_NULL_HANDLE;
    PFN_vkDestroyDescriptorPool destroyPool = nullptr;
    std::unique_ptr<Graphics::DeviceBuffer> buffer;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
    std::uint64_t slots = 0;
    std::uint64_t headerBytes = 0;
    std::uint64_t bytes = 0;
    std::uint64_t lastUse = 0;
    ~ScratchPool() {
        buffer.reset();
        if (pool != VK_NULL_HANDLE) destroyPool(device, pool, nullptr);
    }
};

void ZeroScratchPoolHeader(const Graphics::Context& context, const ScratchPool& pool) {
    Graphics::CommandBatch batch(context);
    context.Resolved(&Graphics::DeviceFunctions::cmdFillBuffer, "vkCmdFillBuffer")(batch.Handle(), pool.buffer->Handle(), 0, pool.headerBytes, 0u);
    Graphics::RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    batch.SubmitAndWait();
}

std::uint64_t MebibytesFromEnvironment(const char* name, std::uint64_t fallback) {
    std::uint64_t mebibytes = fallback;
    if (const char* text = std::getenv(name)) {
        char* end = nullptr;
        const auto parsed = std::strtoull(text, &end, 10);
        if (end != text && *end == 0 && parsed != 0) mebibytes = parsed;
    }
    return mebibytes << 20u;
}

std::uint64_t ScratchPoolCapBytes() {
    static const std::uint64_t bytes = MebibytesFromEnvironment("APS5_SCRATCH_POOL_MIB", 64);
    return bytes;
}

constexpr std::uint64_t ScratchPoolBufferLimit = std::uint64_t{1} << 31u;

std::uint64_t ScratchPoolTotalBytes() {
    static const std::uint64_t bytes = MebibytesFromEnvironment("APS5_SCRATCH_POOL_TOTAL_MIB", 256);
    return bytes;
}

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
    std::uint64_t presentedAddress = 0;
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
    bool tessellationPipelineCache = true;
    PFN_vkGetCalibratedTimestampsKHR calibratedTimestamps = nullptr;
    PFN_vkCmdSetCheckpointNV checkpointMarker = nullptr;
    PFN_vkCmdWriteBufferMarkerAMD bufferMarker = nullptr;
    VkBuffer markerBuffer = VK_NULL_HANDLE;
    VkDeviceMemory markerMemory = VK_NULL_HANDLE;
    void* markerMapping = nullptr;
    bool deviceLostHook = false;
    std::atomic<bool> hookRetired{false};
    std::atomic<unsigned> hookActive{0};
    bool deferDestruction = false;
    PFN_vkGetDeviceProcAddr contextDeviceProc = nullptr;
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
    bool fragmentBarycentricBuiltins = false;
    bool hostInterpolationFirst = false;
    bool geometryShader = false;
    bool multiViewport = false;
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
    bool float32FlushComputeOnly = false;
    bool float16WidenNans = false;
    bool float16NarrowNans = false;
    bool float32DenormPreserve = false;
    bool float16HostNanCandidate = false;
    bool float16HostNans = false;
    bool probingFloatControls = false;
    bool demoteToHelperInvocation = false;
    bool earlyFragmentSampleMaskTestBeforeSampleCounting = false;
    bool robustBufferAccess2 = false;
    bool exactRobustLoads = false;
    bool subgroupControlBarriers = true;
    bool workgroupScopeFences = true;
    bool ldsLockFences = false;
    bool deviceAtomicWorkgroupMemoryBarriers = false;
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
    bool fmaFloat16 = false;
    bool primitiveListRestart = false;
    bool depthClipControl = false;
    bool graphicsPipelineLibrary = false;
    bool pipelineLinkTimeOptimization = true;
    bool dynamicColorWriteMask = false;
    bool imageViewMinLod = false;
    bool shaderResourceMinLod = false;
    bool shaderImageMinLod = false;
    bool pipelineExecutableInfo = false;
    bool maintenance8 = false;
    bool pushDescriptors = false;
    bool depthUploadImage = false;
    bool maintenance5 = false;
    bool fillModeNonSolid = false;
    bool wideLines = false;
    std::uint32_t srgbDecodeFormats = 0;
    std::uint32_t srgbCopyFormats = 0;
    std::uint32_t scaledSampledFormats = 0;
    bool storageImageWithoutFormat = false;
    VkSampleCountFlags sampleLocationSampleCounts = 0;
    VkSampleCountFlags sampleLocationGridSampleCounts = 0;
    std::array<VkExtent2D, 4> sampleLocationGridSizes{};
    bool shaderStencilExport = false;
    bool depthClamp = false;
    bool depthBounds = false;
    bool depthBiasClamp = false;
    bool dualSrcBlend = false;
    bool logicOp = false;
    bool occlusionQueryPrecise = false;
    VkDeviceSize hostImportAlignment = 0;
    bool drawInputsInPlace = true;
    bool hostPointerImport = false;
    std::size_t drawInputsInPlaceBelow = 0;
    bool dmaBufImport = false;
    bool sparseBinding = false;
    bool sparseResidencyBuffer = false;
    bool sparseResidencyAliased = false;
    bool sparseResidencyImage2D = false;
    std::uint32_t queueFamily = 0;
    std::uint32_t sparseFamily = VK_QUEUE_FAMILY_IGNORED;
    std::uint32_t sparseIndex = 0;
    VkQueue sparseQueue = VK_NULL_HANDLE;
    std::mutex sparseQueueMutex;
    VkQueue settleQueue = VK_NULL_HANDLE;
    std::uint32_t settleFamily = 0;
    VkCommandPool settlePool = VK_NULL_HANDLE;
    std::mutex settleQueueMutex;
    VkQueue presentQueue = VK_NULL_HANDLE;
    std::mutex presentQueueMutex;
    bool memoryBudget = false;
    bool depthRangeUnrestricted = false;
    bool samplerAnisotropy = false;
    bool textureCompressionBC = false;
    bool samplerFilterMinmax = false;
    bool samplerMirrorClampToEdge = false;
    bool nonSeamlessCubeMap = false;
    VkSampleCountFlags mergedCoverageSampleCounts = 0;
    VkSampleCountFlags truncatedCoverageSampleCounts = 0;
    bool mixedAttachmentSamples = false;
    bool fragmentShaderPixelInterlock = false;
    bool conservativeRasterization = false;
    bool provokingVertexLast = false;
    bool provokingVertexModePerPipeline = false;
    bool timelineSemaphores = false;
    bool computeWave32 = false;
    bool meshWave32 = false;
    std::uint32_t maxComputeSubgroupSize = 0;
    std::uint32_t minComputeSubgroupSize = 0;
    bool spannedAnySubgroup = false;
    bool computeFullSubgroups = false;
    bool spannedWaves = false;
    bool drawIndirectFirstInstance = false;
    bool multiDrawIndirect = false;
    bool drawIndirectCount = false;
    bool indexedIndirectPatches = true;
    Graphics::DeviceFunctions deviceFunctions;
    bool functionsReady = false;
    Graphics::Context context;
    bool contextReady = false;
    std::unique_ptr<Graphics::TextureDetiler> detiler;
    std::unique_ptr<Graphics::Recorder> recorder;
    std::vector<std::pair<std::array<std::uint32_t, 4>, std::shared_ptr<Graphics::DeviceBuffer>>> patternBuffers;
    struct RepeatedFill {
        std::size_t bytes = 0;
        std::array<std::uint32_t, 4> pattern{};
        std::uint64_t generation = 0;
        std::uint64_t importSerial = 0;
        std::uint64_t unwatchSerial = 0;
    };
    std::unordered_map<std::uint64_t, RepeatedFill> repeatedFills;
    VkDescriptorSetLayout workgroupMemoryLayout = VK_NULL_HANDLE;
    std::shared_ptr<WorkgroupMemory> workgroupMemory;
    std::mutex scratchPoolsMutex;
    std::map<ShaderRecompiler::PipelineIdentity, std::shared_ptr<ScratchPool>> scratchPools;
    std::uint64_t scratchPoolUses = 0;
    AgcDriver::Mutex computePipelinesMutex;
    std::map<ComputePipelineKey, std::shared_ptr<ComputePipelineObjects>> computePipelines;
    std::map<ComputePipelineKey, std::shared_future<std::shared_ptr<ComputePipelineObjects>>> computePipelinesBuilding;
    std::shared_ptr<Graphics::PipelineJournalReplay> journalReplay;
    std::atomic<bool> journalReplayPending{false};
    std::atomic<std::uint64_t> computePipelinesCreated{0};
    std::unique_ptr<Graphics::GpuColorTransfer> colorTransfer;
    std::shared_ptr<Graphics::BufferPool> bufferPool;
    std::unique_ptr<Graphics::Buffer> emptyBuffer;
    std::unique_ptr<Graphics::Buffer> oversizedBuffer;
    std::unique_ptr<Graphics::Buffer> atomicLockTable;
    std::unique_ptr<Graphics::PlaceholderImages> placeholderImages;
    std::unique_ptr<Graphics::TextureCache> textureCache;
    std::unique_ptr<Graphics::PipelineCache> pipelineCache;
    std::unique_ptr<Graphics::DescriptorCache> descriptorCache;
    std::unique_ptr<Graphics::VertexInputCache> vertexInputs;
    Graphics::VertexBufferFormats vertexFormats;
    std::unique_ptr<Graphics::SamplerCache> samplerCache;
    Graphics::ResourceCache& resourceCache = Graphics::SharedResourceCache();
    std::shared_ptr<std::vector<std::shared_ptr<Graphics::ShaderResources>>> copiedWriters = std::make_shared<std::vector<std::shared_ptr<Graphics::ShaderResources>>>();
    VkPhysicalDeviceMeshShaderPropertiesEXT meshLimits{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT};
    std::unique_ptr<PresentationScaler> scaler;
    std::unique_ptr<PresentationPass> presentation;
    std::unique_ptr<PresentationPass> planePass;
    std::unique_ptr<PerformanceOverlay> overlay;
    std::unique_ptr<PresentationUpscaler> upscaler;
    std::unique_ptr<PerformanceOverlay> keyboard;
    std::unique_ptr<PerformanceOverlay> padStatus;
    VkFormat surfaceFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkColorSpaceKHR surfaceColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    bool swapchainColorSpaces = false;
    bool hdrMetadata = false;
    std::vector<VkSurfaceFormatKHR> surfaceFormats;
    std::uint64_t surfaceDisplayGeneration = 0;
    std::uint64_t metadataGeneration = 0;
    std::unique_ptr<PresentationScaler> hdrScaler;
    VkFormat hdrScalerSource = VK_FORMAT_UNDEFINED;
    bool imageAcquired = false;
    std::uint32_t acquiredIndex = 0;
    struct PresentSlot {
        VkFence fence = VK_NULL_HANDLE;
        VkCommandBuffer commands = VK_NULL_HANDLE;
        std::shared_ptr<void> kept;
        std::shared_ptr<void> keptPlane;
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
        bool shared = false;
        std::uint64_t recorderSerial = 0;
        std::uint64_t previousSerial = 0;
        std::chrono::steady_clock::time_point submittedAt{};
        VkQueryPool queries = VK_NULL_HANDLE;
        std::uint64_t blitBytes = 0;
    };
    std::vector<PresentSlot> presentSlots;
    std::size_t presentCursor = 0;
    std::uint64_t lastPresentSerial = 0;
    struct RetiredRange {
        std::uint64_t afterSerial;
        std::uint64_t throughSerial;
    };
    std::deque<RetiredRange> retiredRanges;
    AgcDriver::Mutex presentMutex;
    bool queuePending = false;
    bool queueGuestFrame = false;
    bool presentingGuestFrame = false;
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
    TFunction DeviceFunction(Graphics::FunctionName name) const {
        ++Graphics::DeviceProcLookups();
        auto function = reinterpret_cast<TFunction>(deviceProc(device, name.Value()));
        if (function == nullptr) {
            throw std::runtime_error(std::string("Vulkan device function missing: ") + name.Value());
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

    std::shared_ptr<ScratchPool> ScratchPoolFor(const Graphics::Context& context, const ShaderRecompiler::PipelineIdentity& identity, std::uint32_t slotDwords, std::uint64_t groups) {
        const auto range = std::min<std::uint64_t>(properties.limits.maxStorageBufferRange, ScratchPoolBufferLimit);
        const auto slotBytes = (static_cast<std::uint64_t>(slotDwords) + 2u) * 4u;
        if (slotBytes > range) {
            throw std::runtime_error("Vulkan dispatch: one scratch pool slot of " + std::to_string(slotBytes) + " bytes is over the scratch pool buffer limit of " + std::to_string(range) + " bytes");
        }
        const auto capSlots = std::max<std::uint64_t>(std::min({ScratchPoolCapBytes(), ScratchPoolTotalBytes(), range}) / slotBytes, 1u);
        const auto wanted = std::clamp<std::uint64_t>(groups, 1u, capSlots);
        std::lock_guard lock(scratchPoolsMutex);
        const auto use = ++scratchPoolUses;
        if (const auto found = scratchPools.find(identity); found != scratchPools.end() && found->second->slots >= wanted) {
            found->second->lastUse = use;
            return found->second;
        }
        const auto size = wanted * slotBytes;
        EvictScratchPools(identity, size);
        auto created = std::make_shared<ScratchPool>();
        created->device = device;
        created->destroyPool = DeviceFunction<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool");
        created->slots = wanted;
        created->headerBytes = wanted * 2u * 4u;
        created->bytes = size;
        created->lastUse = use;
        created->buffer = std::make_unique<Graphics::DeviceBuffer>(context, static_cast<std::size_t>(size), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        check(DeviceFunction<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(device, &poolInfo, nullptr, &created->pool), "vkCreateDescriptorPool scratch pool");
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocation.descriptorPool = created->pool;
        allocation.descriptorSetCount = 1;
        allocation.pSetLayouts = &workgroupMemoryLayout;
        check(DeviceFunction<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(device, &allocation, &created->set), "vkAllocateDescriptorSets scratch pool");
        const VkDescriptorBufferInfo described{created->buffer->Handle(), 0, size};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = created->set;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.pBufferInfo = &described;
        DeviceFunction<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(device, 1, &write, 0, nullptr);
        ZeroScratchPoolHeader(context, *created);
        scratchPools[identity] = created;
        return created;
    }

    void EvictScratchPools(const ShaderRecompiler::PipelineIdentity& identity, std::uint64_t needed) {
        scratchPools.erase(identity);
        std::uint64_t held = 0;
        for (const auto& [key, pool] : scratchPools) held += pool->bytes;
        while (!scratchPools.empty() && held + needed > ScratchPoolTotalBytes()) {
            const auto oldest = std::min_element(scratchPools.begin(), scratchPools.end(), [](const auto& left, const auto& right) { return left.second->lastUse < right.second->lastUse; });
            held -= oldest->second->bytes;
            scratchPools.erase(oldest);
        }
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
        Graphics::MemoryAccounting::Record(device, markerMemory, allocation, memoryProperties, "buffer markers");
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
            if (uploadMemory) Graphics::MemoryAccounting::Free(DeviceFunction<PFN_vkFreeMemory>("vkFreeMemory"), device, uploadMemory);
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
            Graphics::MemoryAccounting::Record(device, uploadMemory, allocation, memoryProperties, "display upload");
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

    void RetireSlot(PresentSlot& slot) {
        slot.inFlight = false;
        if (presentation) presentation->Release(static_cast<std::size_t>(&slot - presentSlots.data()));
        if (upscaler) upscaler->Release(static_cast<std::size_t>(&slot - presentSlots.data()));
        if (planePass) planePass->Release(static_cast<std::size_t>(&slot - presentSlots.data()));
        slot.kept.reset();
        slot.keptPlane.reset();
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
            if (stamped && missing == 0) Graphics::Recorder::NoteFrameGpuBusy(busyMs);
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
            dumpWriter.Enqueue(index, slot.dumpWidth, slot.dumpHeight, true, std::vector<std::byte>(full.begin(), full.end()));
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
        AgcDriver::ProfilePrint_nid_no_patch("[gpu] frame_%03d.bmp: %ux%u display read back on the GPU at %.1f s\n", index, slot.dumpWidth, slot.dumpHeight, std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
        dumpWriter.Enqueue(index, width, height, false, std::move(pixels));
    }

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
            hookRetired.store(true, std::memory_order_release);
            while (hookActive.load(std::memory_order_acquire) != 0) std::this_thread::yield();
        }
        Graphics::ResetCheckpointSnapshots();
        if (compileOnly && device != VK_NULL_HANDLE) {
            reinterpret_cast<PFN_vkDestroyDevice>(deviceProc(device, "vkDestroyDevice"))(device, nullptr);
            device = VK_NULL_HANDLE;
        }
        if (device != VK_NULL_HANDLE) {
            const auto idle = reinterpret_cast<PFN_vkDeviceWaitIdle>(deviceProc(device, "vkDeviceWaitIdle"))(device);
            if (idle != VK_SUCCESS && idle != VK_ERROR_DEVICE_LOST) std::terminate();
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
            Graphics::ClearCachedPipelines(device);
            Graphics::ClearDepthSurfaces(device);
            Graphics::ClearMultisampleTargets(device);
            Graphics::ClearSampleOrders(device);
            Graphics::ClearStagedMerges(device);
            {
                std::lock_guard pipelines(computePipelinesMutex);
                computePipelines.clear();
                computePipelinesBuilding.clear();
            }
            copiedWriters->clear();
            resourceCache.Clear();
            Graphics::ClearRetainedSpaces(device);
            Graphics::ClearCachedTextures(device);
            Graphics::ClearSnapshotCopies(device);
            Graphics::ClearUploadCopies(device);
            Graphics::ClearImageMirrors(device);
            Graphics::ClearHostImports(device);
            patternBuffers.clear();
            repeatedFills.clear();
            workgroupMemory.reset();
            scratchPools.clear();
            if (workgroupMemoryLayout != VK_NULL_HANDLE) reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(deviceProc(device, "vkDestroyDescriptorSetLayout"))(device, workgroupMemoryLayout, nullptr);
            descriptorCache.reset();
            vertexInputs.reset();
            emptyBuffer.reset();
            oversizedBuffer.reset();
            atomicLockTable.reset();
            placeholderImages.reset();
            samplerCache.reset();
            textureCache.reset();
            detiler.reset();
            colorTransfer.reset();
            overlay.reset();
            upscaler.reset();
            planePass.reset();
            keyboard.reset();
            padStatus.reset();
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
            if (uploadMemory) Graphics::MemoryAccounting::Free(reinterpret_cast<PFN_vkFreeMemory>(deviceProc(device, "vkFreeMemory")), device, uploadMemory);
            if (markerMapping) reinterpret_cast<PFN_vkUnmapMemory>(deviceProc(device, "vkUnmapMemory"))(device, markerMemory);
            if (markerBuffer) reinterpret_cast<PFN_vkDestroyBuffer>(deviceProc(device, "vkDestroyBuffer"))(device, markerBuffer, nullptr);
            if (markerMemory) Graphics::MemoryAccounting::Free(reinterpret_cast<PFN_vkFreeMemory>(deviceProc(device, "vkFreeMemory")), device, markerMemory);
            Graphics::TeardownCheckpointMarkers();
            if (swapchain) reinterpret_cast<PFN_vkDestroySwapchainKHR>(deviceProc(device, "vkDestroySwapchainKHR"))(device, swapchain, nullptr);
            const auto destroyPool = reinterpret_cast<PFN_vkDestroyCommandPool>(deviceProc(device, "vkDestroyCommandPool"));
            const auto destroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(deviceProc(device, "vkDestroyDevice"));
            if (pool != VK_NULL_HANDLE) {
                destroyPool(device, pool, nullptr);
            }
            if (settlePool != VK_NULL_HANDLE) destroyPool(device, settlePool, nullptr);
            Graphics::DestroyDeferredObjects(device);
            destroyDevice(device, nullptr);
            Graphics::MemoryAccounting::ForgetDevice(device);
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
    require(capture.stageFlags == 0 || state->computeFullSubgroups, "captured full compute subgroups are unsupported");
    if (capture.requiredSubgroupSize != 0) {
        VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT};
        VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &subgroup};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(state->physical, &properties);
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
    const Graphics::PipelineSpecialization specialization(shader, graphicsContext().atomicLockTable);
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
    if ((capture.flags & VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR) != 0) Graphics::LogPipelineStatistics_nid_no_patch(graphicsContext(), objects.pipeline);
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
    const bool hostReport = !compileOnly && ParseHostReportSetting(std::getenv("ANYPS5_HOST_REPORT"));
    APS5_LOG_OUT("VulkanDevice constructor window=%p", static_cast<const void*>(window));
#ifdef _WIN32
    state->library = SDL_LoadObject("vulkan-1.dll");
#elif defined(__APPLE__)
    state->library = SDL_LoadObject("libvulkan.1.dylib");
#else
    state->library = SDL_LoadObject("libvulkan.so.1");
#endif
    if (state->library == nullptr) {
        throw VulkanUnavailable(std::string("Vulkan loader: ") + SDL_GetError());
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
    }
#ifdef __APPLE__
    instanceExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    create.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif
    create.enabledExtensionCount = static_cast<std::uint32_t>(instanceExtensions.size());
    create.ppEnabledExtensionNames = instanceExtensions.empty() ? nullptr : instanceExtensions.data();
    const VkResult instanceResult = state->InstanceFunction<PFN_vkCreateInstance>("vkCreateInstance")(&create, nullptr, &state->instance);
    if (instanceResult == VK_ERROR_INCOMPATIBLE_DRIVER) throw VulkanUnavailable("vkCreateInstance: Vulkan result " + std::to_string(instanceResult));
    check(instanceResult, "vkCreateInstance");
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
    std::vector<std::string> candidateList;
    std::string rejectedHardwareName;
    PresentationRejection rejectedHardwareReason = PresentationRejection::None;
    int rejectedHardwareRank = -1;
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
        candidateList.emplace_back(properties.deviceName);
        if (properties.apiVersion < VK_API_VERSION_1_1) {
            continue;
        }
        if (requestedName != nullptr && !matchesRequest(properties.deviceName)) continue;
        const int rank = requestedName != nullptr ? 4 : rankDeviceType(properties.deviceType);
        if (rank <= selectedRank) {
            continue;
        }
        bool swapchain = true;
        if (window != nullptr) {
            std::uint32_t extensionCount = 0;
            auto enumerateExtensions = state->InstanceFunction<PFN_vkEnumerateDeviceExtensionProperties>("vkEnumerateDeviceExtensionProperties");
            check(enumerateExtensions(physical, nullptr, &extensionCount, nullptr), "vkEnumerateDeviceExtensionProperties");
            std::vector<VkExtensionProperties> extensions(extensionCount);
            check(enumerateExtensions(physical, nullptr, &extensionCount, extensions.data()), "vkEnumerateDeviceExtensionProperties");
            swapchain = std::all_of(presentationExtensions.begin(), presentationExtensions.end(), [&](const char* name) {
                return std::any_of(extensions.begin(), extensions.end(), [&](const auto& item) { return std::strcmp(item.extensionName, name) == 0; });
            });
        }
        std::uint32_t families = 0;
        auto getFamilies = state->InstanceFunction<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");
        getFamilies(physical, &families, nullptr);
        std::vector<VkQueueFamilyProperties> queues(families);
        getFamilies(physical, &families, queues.data());
        bool graphicsCompute = false;
        bool presentable = false;
        for (std::uint32_t i = 0; i < families; ++i) {
            if (queues[i].queueCount != 0 && (queues[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) == (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
                graphicsCompute = true;
                if (window != nullptr) {
                    if (!swapchain) continue;
                    VkBool32 supported = VK_FALSE;
                    check(state->InstanceFunction<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>("vkGetPhysicalDeviceSurfaceSupportKHR")(physical, i, state->surface, &supported), "vkGetPhysicalDeviceSurfaceSupportKHR");
                    if (!supported) continue;
                }
                presentable = true;
                selected = physical;
                family = i;
                selectedRank = rank;
                break;
            }
        }
        if (window != nullptr && requestedName == nullptr && rank >= 2 && rank > rejectedHardwareRank && graphicsCompute && !presentable) {
            rejectedHardwareName = properties.deviceName;
            rejectedHardwareReason = swapchain ? PresentationRejection::NoSurfaceQueue : PresentationRejection::MissingSwapchain;
            rejectedHardwareRank = rank;
        }
    }
    if (selected == VK_NULL_HANDLE && requestedName != nullptr) {
        throw std::runtime_error(std::string("Vulkan: no usable device whose name contains ANYPS5_GPU=\"") + requestedName + "\"; devices: " + candidateNames);
    }
    if (selected == VK_NULL_HANDLE) {
        throw VulkanUnavailable(window ? "Vulkan: no Vulkan 1.1 device with graphics, compute and swapchain presentation" : "Vulkan: no Vulkan 1.1 graphics and compute queue");
    }
    APS5_LOG_OUT("Physical device selected physical=%p queueFamily=%u", reinterpret_cast<void*>(selected), family);
    VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    properties.pNext = &state->subgroup;
    state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &properties);
    state->properties = properties.properties;
    state->physical = selected;
    if (window != nullptr) {
        const auto warning = SoftwareFallbackWarning(state->properties.deviceType, state->properties.deviceName, rejectedHardwareName, rejectedHardwareReason, requestedName != nullptr);
        if (!warning.empty()) std::fprintf(stderr, "%s\n", warning.c_str());
    }
    state->srgbDecodeFormats = Graphics::SrgbDecodeFormats(state->InstanceFunction<PFN_vkGetPhysicalDeviceFormatProperties>("vkGetPhysicalDeviceFormatProperties"), selected);
    state->scaledSampledFormats = Graphics::ScaledSampledFormats(state->InstanceFunction<PFN_vkGetPhysicalDeviceFormatProperties>("vkGetPhysicalDeviceFormatProperties"), selected);
    state->srgbCopyFormats = Graphics::SrgbCopyFormats(state->InstanceFunction<PFN_vkGetPhysicalDeviceFormatProperties>("vkGetPhysicalDeviceFormatProperties"), selected, state->srgbDecodeFormats);
    if ((state->subgroup.supportedOperations & VK_SUBGROUP_FEATURE_BASIC_BIT) != 0) {
        state->capabilities.push_back(spv::CapabilityGroupNonUniform);
        if ((state->subgroup.supportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT) != 0) state->capabilities.push_back(spv::CapabilityGroupNonUniformBallot);
        if ((state->subgroup.supportedOperations & VK_SUBGROUP_FEATURE_SHUFFLE_BIT) != 0) state->capabilities.push_back(spv::CapabilityGroupNonUniformShuffle);
        if ((state->subgroup.supportedOperations & VK_SUBGROUP_FEATURE_ARITHMETIC_BIT) != 0) state->capabilities.push_back(spv::CapabilityGroupNonUniformArithmetic);
        if ((state->subgroup.supportedOperations & VK_SUBGROUP_FEATURE_QUAD_BIT) != 0) state->capabilities.push_back(spv::CapabilityGroupNonUniformQuad);
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
        state->deferDestruction = driver.driverID == VK_DRIVER_ID_MOLTENVK && std::getenv("APS5_NO_DEFERRED_DESTRUCTION") == nullptr;
        state->driverInfo = std::string(driver.driverName) + " " + driver.driverInfo;
        state->driverId = driver.driverID;
        state->depthUploadImage = driver.driverID == VK_DRIVER_ID_AMD_PROPRIETARY && state->properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU && std::getenv("APS5_NO_DEPTH_UPLOAD_IMAGE") == nullptr;
        state->individuallyFreeDescriptorSets = driver.driverID == VK_DRIVER_ID_MESA_LLVMPIPE;
        state->fusedExactArithmetic = driver.driverID == VK_DRIVER_ID_MOLTENVK;
        state->tessellationPipelineCache = TessellationPipelineCacheUsable(driver.driverID);
        state->indexedIndirectPatches = IndexedIndirectPatchesUsable(driver.driverID);
        state->hostInterpolationFirst = HostInterpolationPreferred(driver.driverID) && std::getenv("APS5_NO_HOST_INTERPOLATION_FIRST") == nullptr;
        APS5_LOG_OUT("Pixel interpolation driver=%d hostInterpolationFirst=%d", static_cast<int>(driver.driverID), state->hostInterpolationFirst ? 1 : 0);
        state->subgroupControlBarriers = SubgroupControlBarrierUsable(driver.driverID);
        state->workgroupScopeFences = WorkgroupScopeFenceUsable(driver.driverID);
        state->ldsLockFences = LdsLockFencesRequired(driver.driverID);
        state->deviceAtomicWorkgroupMemoryBarriers = DeviceAtomicWorkgroupMemoryBarriersRequired(driver.driverID);
    }
    {
        const auto driverText = state->driverInfo.empty() ? std::to_string(state->properties.driverVersion) : state->driverInfo;
        LibcRunReportSetGpu_nid_no_patch(state->properties.vendorID, state->properties.deviceName, driverText.c_str(), state->properties.apiVersion);
        HostGpuQuery hostQuery;
        hostQuery.instanceProc = state->instanceProc;
        hostQuery.instance = state->instance;
        hostQuery.physical = selected;
        hostQuery.surface = state->surface;
        hostQuery.presentation = window != nullptr;
        hostQuery.subgroupSize = state->subgroup.subgroupSize;
        hostQuery.extensions = availableExtensions;
        hostQuery.candidates = candidateList;
        hostQuery.requestedPresentMode = std::getenv("ANYPS5_PRESENT_MODE");
        checkHostRequirements(hostQuery, hostReport);
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
        state->fragmentBarycentricBuiltins = state->fragmentShaderBarycentric;
        if (state->fragmentShaderBarycentric && hasExtension(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME)) {
            state->fragmentShaderBarycentric = FragmentShaderBarycentricUsable(driverId, state->fragmentShaderBarycentric);
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
        state->narrowSubgroupClock = NarrowSubgroupClock(driverId, state->properties.deviceID, state->properties.deviceName);
        APS5_LOG_OUT("Shader clock driver=%d narrowSubgroupClock=%d", static_cast<int>(driverId), state->narrowSubgroupClock ? 1 : 0);
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
    if (state->fragmentBarycentricBuiltins) {
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
    state->float16HostNanCandidate = float16Rte && std::getenv("APS5_NO_HOST_F16_NANS") == nullptr;
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
    state->float32FlushComputeOnly = float32Flush.computeOnly;
    if (state->float32FlushToZeroCandidate && float32Flush.declared) state->capabilities.push_back(spv::CapabilityDenormFlushToZero);
    state->float32DenormPreserve = floatControls.shaderDenormPreserveFloat32 == VK_TRUE && floatControls.denormBehaviorIndependence != VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_NONE && std::getenv("APS5_NO_HW_F32_PRESERVE") == nullptr;
    if ((state->float16DenormPreserve || state->float32DenormPreserve) && std::find(state->capabilities.begin(), state->capabilities.end(), static_cast<std::uint32_t>(spv::CapabilityDenormPreserve)) == state->capabilities.end()) state->capabilities.push_back(spv::CapabilityDenormPreserve);
    VkPhysicalDeviceShaderFmaFeaturesKHR fmaFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FMA_FEATURES_KHR};
    if (float16Rte && hasExtension(VK_KHR_SHADER_FMA_EXTENSION_NAME)) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &fmaFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
    }
    const bool float16Fma = float16Rte && fmaFeatures.shaderFmaFloat16 == VK_TRUE;
    fmaFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FMA_FEATURES_KHR};
    fmaFeatures.shaderFmaFloat16 = VK_TRUE;
    state->fmaFloat16 = float16Fma;
    if (float16Fma) {
        deviceExtensions.push_back(VK_KHR_SHADER_FMA_EXTENSION_NAME);
        state->capabilities.push_back(spv::CapabilityFMAKHR);
        state->spirvExtensions.push_back("SPV_KHR_fma");
    }
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
    if (window != nullptr && requestedPresentMode != nullptr && std::strcmp(requestedPresentMode, "fifo_latest_ready") == 0 && fifoLatestReadyExtension != nullptr) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &fifoLatestReadyFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        fifoLatestReady = fifoLatestReadyFeatures.presentModeFifoLatestReady == VK_TRUE;
        if (fifoLatestReady) deviceExtensions.push_back(fifoLatestReadyExtension);
    }
    const bool imageRobustness = hasExtension(VK_EXT_IMAGE_ROBUSTNESS_EXTENSION_NAME);
    if (imageRobustness) deviceExtensions.push_back(VK_EXT_IMAGE_ROBUSTNESS_EXTENSION_NAME);
    if (hasExtension(VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME)) {
        VkPhysicalDeviceSamplerFilterMinmaxPropertiesEXT minmaxProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES_EXT};
        VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &minmaxProperties};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &properties);
        state->samplerFilterMinmax = minmaxProperties.filterMinmaxSingleComponentFormats == VK_TRUE && minmaxProperties.filterMinmaxImageComponentMapping == VK_TRUE;
        if (state->samplerFilterMinmax) deviceExtensions.push_back(VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME);
    }
    state->samplerMirrorClampToEdge = hasExtension(VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME);
    if (state->samplerMirrorClampToEdge) deviceExtensions.push_back(VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME);
    if (hasExtension(VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME)) {
        VkPhysicalDeviceConservativeRasterizationPropertiesEXT conservativeProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONSERVATIVE_RASTERIZATION_PROPERTIES_EXT};
        VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &conservativeProperties};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &properties);
        state->conservativeRasterization = conservativeProperties.primitiveOverestimationSize <= 1.0f / 256.0f && conservativeProperties.degenerateTrianglesRasterized == VK_TRUE;
        if (state->conservativeRasterization) deviceExtensions.push_back(VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME);
    }
    state->drawIndirectCount = hasExtension(VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME);
    if (state->drawIndirectCount) deviceExtensions.push_back(VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME);
    state->memoryBudget = hasExtension(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
    const bool importsAllowed = std::getenv("APS5_NO_HOST_IMPORT") == nullptr;
    VkPhysicalDeviceMaintenance3Properties maintenance3{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES};
    VkPhysicalDeviceDriverProperties driverProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES, &maintenance3};
    VkPhysicalDeviceExternalMemoryHostPropertiesEXT hostProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT, &driverProperties};
    state->hostPointerImport = importsAllowed && hasExtension(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME);
    if (importsAllowed) {
        VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, state->hostPointerImport ? static_cast<void*>(&hostProperties) : static_cast<void*>(&driverProperties)};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &properties);
    }
#ifndef _WIN32
    state->dmaBufImport = importsAllowed && driverProperties.driverID != VK_DRIVER_ID_NVIDIA_PROPRIETARY && hasExtension(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME) && hasExtension(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME);
#endif
    if (state->hostPointerImport || state->dmaBufImport) {
#ifdef _WIN32
        state->hostImportAlignment = hostProperties.minImportedHostPointerAlignment;
        constexpr bool windows = true;
#else
        state->hostImportAlignment = state->hostPointerImport ? hostProperties.minImportedHostPointerAlignment : static_cast<VkDeviceSize>(sysconf(_SC_PAGESIZE));
        constexpr bool windows = false;
#endif
        state->drawInputsInPlace = std::getenv("APS5_NO_DRAW_INPUTS_IN_PLACE") == nullptr && (std::getenv("APS5_DRAW_INPUTS_IN_PLACE") != nullptr || DrawInputsInPlaceUsable(driverProperties.driverID, windows));
        const char* inPlaceBelow = std::getenv("APS5_DRAW_INPUTS_IN_PLACE_BELOW");
        state->drawInputsInPlaceBelow = std::getenv("APS5_NO_DRAW_INPUTS_IN_PLACE") != nullptr ? 0 : inPlaceBelow != nullptr ? static_cast<std::size_t>(std::strtoull(inPlaceBelow, nullptr, 10)) : DrawInputsInPlaceBelow(driverProperties.driverID, windows);
        if (!compileOnly) Graphics::SetHostImportWindow(8ull << 20u);
        if (!compileOnly) Graphics::SetHostImportMaxBytes(maintenance3.maxMemoryAllocationSize);
        if (!compileOnly) Pm4::RequestImportableGds(state->hostPointerImport && state->properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);
#ifdef _WIN32
        if (const char* value = std::getenv("APS5_WORKING_SET_MIB"); value != nullptr && !compileOnly) {
            const SIZE_T bytes = static_cast<SIZE_T>(std::strtoull(value, nullptr, 10)) << 20u;
            if (!SetProcessWorkingSetSizeEx(GetCurrentProcess(), bytes, bytes * 2, QUOTA_LIMITS_HARDWS_MIN_DISABLE | QUOTA_LIMITS_HARDWS_MAX_DISABLE)) std::fprintf(stderr, "[gpu] SetProcessWorkingSetSizeEx failed: %lu\n", GetLastError());
        }
#endif
        if (state->hostPointerImport) deviceExtensions.push_back(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME);
#ifndef _WIN32
        if (state->dmaBufImport) {
            deviceExtensions.push_back(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
            deviceExtensions.push_back(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME);
        }
#endif
    }
    VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT listRestartFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVE_TOPOLOGY_LIST_RESTART_FEATURES_EXT};
    if (hasExtension(VK_EXT_PRIMITIVE_TOPOLOGY_LIST_RESTART_EXTENSION_NAME) && std::getenv("APS5_NO_LIST_RESTART") == nullptr) {
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
        state->imageViewMinLod = minLodFeatures.minLod == VK_TRUE && std::getenv("APS5_NO_IMAGE_VIEW_MIN_LOD") == nullptr;
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
    state->pushDescriptors = hasExtension(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
    if (state->pushDescriptors) deviceExtensions.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
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
    VkPhysicalDevicePortabilitySubsetFeaturesKHR portabilityFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR};
    const bool portabilitySubset = hasExtension(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
    if (portabilitySubset) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &portabilityFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        deviceExtensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
    }
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
            state->pipelineLinkTimeOptimization = PipelineLinkTimeOptimizationUsable(driverId);
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
    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT dynamicState3Features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT};
    if (hasExtension(VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME)) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &dynamicState3Features};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        state->dynamicColorWriteMask = dynamicState3Features.extendedDynamicState3ColorWriteMask == VK_TRUE && std::getenv("APS5_NO_DYNAMIC_COLOR_WRITE_MASK") == nullptr;
        if (state->dynamicColorWriteMask) deviceExtensions.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME);
    }
    dynamicState3Features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT};
    dynamicState3Features.extendedDynamicState3ColorWriteMask = VK_TRUE;
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
    const float priorities[3]{1.0f, 1.0f, 1.0f};
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
    enabled.shaderInt16 = available.shaderInt16;
    if (enabled.shaderInt16) state->capabilities.push_back(spv::CapabilityInt16);
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
    require(available.independentBlend, "device lacks independentBlend, required for color targets with different blend states");
    enabled.independentBlend = VK_TRUE;
    enabled.robustBufferAccess = available.robustBufferAccess;
    const auto plan = PlanDeviceQueues(queues, family, settles, available.sparseBinding == VK_TRUE, window != nullptr);
    const bool settleQueue = plan.settle;
    const auto settleFamily = plan.settleFamily;
    const auto settleIndex = plan.settleIndex;
    VkDeviceQueueCreateInfo queueInfos[3]{queueInfo, {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}, {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}};
    queueInfos[0].queueCount = plan.mainCount;
    queueInfos[0].pQueuePriorities = plan.mainCount > 1 ? priorities : &priority;
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
    enabled.sparseResidencyImage2D = state->sparseBinding ? available.sparseResidencyImage2D : VK_FALSE;
    state->sparseResidencyImage2D = enabled.sparseResidencyImage2D == VK_TRUE;
    enabled.drawIndirectFirstInstance = available.drawIndirectFirstInstance;
    enabled.multiDrawIndirect = available.multiDrawIndirect;
    state->drawIndirectFirstInstance = enabled.drawIndirectFirstInstance == VK_TRUE;
    state->multiDrawIndirect = enabled.multiDrawIndirect == VK_TRUE;
    enabled.depthClamp = available.depthClamp;
    state->depthClamp = enabled.depthClamp == VK_TRUE;
    enabled.fillModeNonSolid = available.fillModeNonSolid;
    state->fillModeNonSolid = enabled.fillModeNonSolid == VK_TRUE;
    enabled.wideLines = available.wideLines;
    state->wideLines = enabled.wideLines == VK_TRUE;
    enabled.depthBounds = available.depthBounds;
    state->depthBounds = enabled.depthBounds == VK_TRUE;
    enabled.depthBiasClamp = available.depthBiasClamp;
    state->depthBiasClamp = enabled.depthBiasClamp == VK_TRUE;
    enabled.dualSrcBlend = available.dualSrcBlend;
    enabled.logicOp = available.logicOp;
    state->logicOp = enabled.logicOp == VK_TRUE;
    state->dualSrcBlend = enabled.dualSrcBlend == VK_TRUE;
    enabled.occlusionQueryPrecise = available.occlusionQueryPrecise;
    state->occlusionQueryPrecise = enabled.occlusionQueryPrecise == VK_TRUE;
    enabled.shaderStorageImageMultisample = available.shaderStorageImageMultisample;
    state->storageImageMultisample = enabled.shaderStorageImageMultisample == VK_TRUE;
    if (enabled.shaderStorageImageMultisample) {
        state->capabilities.push_back(spv::CapabilityStorageImageMultisample);
        state->capabilities.push_back(spv::CapabilityImageMSArray);
    }
    enabled.shaderStorageImageWriteWithoutFormat = available.shaderStorageImageWriteWithoutFormat;
    state->storageImageWriteWithoutFormat = enabled.shaderStorageImageWriteWithoutFormat == VK_TRUE;
    enabled.shaderStorageImageReadWithoutFormat = available.shaderStorageImageReadWithoutFormat;
    enabled.shaderImageGatherExtended = available.shaderImageGatherExtended;
    if (enabled.shaderImageGatherExtended) state->capabilities.push_back(spv::CapabilityImageGatherExtended);
    enabled.shaderResourceMinLod = available.shaderResourceMinLod;
    if (enabled.shaderResourceMinLod) state->capabilities.push_back(spv::CapabilityMinLod);
    state->shaderResourceMinLod = enabled.shaderResourceMinLod == VK_TRUE;
    state->shaderImageMinLod = state->shaderResourceMinLod && !state->imageViewMinLod && !state->graphicsPipelineLibrary;
    enabled.sampleRateShading = available.sampleRateShading;
    enabled.geometryShader = available.geometryShader;
    state->geometryShader = enabled.geometryShader == VK_TRUE;
    enabled.multiViewport = available.multiViewport && state->properties.limits.maxViewports >= Graphics::MaxViewports ? VK_TRUE : VK_FALSE;
    state->multiViewport = enabled.multiViewport == VK_TRUE;
    if (enabled.multiViewport) state->capabilities.push_back(spv::CapabilityMultiViewport);
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
    if (state->dynamicColorWriteMask) {
        dynamicState3Features.pNext = const_cast<void*>(deviceInfo.pNext);
        deviceInfo.pNext = &dynamicState3Features;
    }
    if (state->maintenance8) {
        maintenance8Features.pNext = const_cast<void*>(deviceInfo.pNext);
        deviceInfo.pNext = &maintenance8Features;
    }
    if (state->nonSeamlessCubeMap) {
        nonSeamlessCubeFeatures.pNext = const_cast<void*>(deviceInfo.pNext);
        deviceInfo.pNext = &nonSeamlessCubeFeatures;
    }
    if (portabilitySubset) {
        portabilityFeatures.pNext = const_cast<void*>(deviceInfo.pNext);
        deviceInfo.pNext = &portabilityFeatures;
    }
    if (!state->mixedAttachmentSamples && (state->mergedCoverageSampleCounts != 0 || state->truncatedCoverageSampleCounts != 0)) {
        coverageReductionFeatures.pNext = const_cast<void*>(deviceInfo.pNext);
        deviceInfo.pNext = &coverageReductionFeatures;
    }
    byteFeatures.pNext = const_cast<void*>(deviceInfo.pNext);
    if (state->fragmentBarycentricBuiltins) {
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
    if (float16Fma) {
        fmaFeatures.pNext = byteFeatures.pNext;
        byteFeatures.pNext = &fmaFeatures;
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
        state->minComputeSubgroupSize = subgroupSize.minSubgroupSize;
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
    state->spannedWaves = state->computeFullSubgroups && std::getenv("APS5_NO_SPANNED_WAVE") == nullptr;
    state->spannedAnySubgroup = state->spannedWaves && state->minComputeSubgroupSize < 32u;
    APS5_LOG_OUT("single-wave wave64 compute spans full subgroups: %d, of any size: %d", state->spannedWaves, state->spannedAnySubgroup);
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
    const char* vendorDumpPath = DeviceFaultVendorDumpPath();
    const bool vendorDumpRequested = vendorDumpPath != nullptr;
    const bool vendorBinarySupported = faultFeatures.deviceFaultVendorBinary == VK_TRUE;
    const bool deviceFault = faultFeatures.deviceFault == VK_TRUE;
    const bool vendorBinary = ChooseDeviceFaultVendorBinary(vendorDumpRequested, deviceFault, vendorBinarySupported);
    if (deviceFault) {
        faultFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT};
        faultFeatures.deviceFault = VK_TRUE;
        faultFeatures.deviceFaultVendorBinary = vendorBinary ? VK_TRUE : VK_FALSE;
        deviceExtensions.push_back(VK_EXT_DEVICE_FAULT_EXTENSION_NAME);
        deviceInfo.enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size());
        deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
        faultFeatures.pNext = byteFeatures.pNext;
        byteFeatures.pNext = &faultFeatures;
    }
    if (vendorBinary) std::fprintf(stderr, "[gpu] APS5_DUMP_DEVICE_FAULT: deviceFaultVendorBinary enabled, vendor binary will be written to %s\n", vendorDumpPath);
    else if (vendorDumpRequested) std::fprintf(stderr, "[gpu] APS5_DUMP_DEVICE_FAULT: the device does not support deviceFaultVendorBinary, no vendor binary will be written\n");
    const bool checkpointsRequested = std::getenv("APS5_GPU_CHECKPOINTS") != nullptr;
    const bool checkpoints = checkpointsRequested && hasExtension(VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME);
    const bool bufferMarkers = checkpointsRequested && !checkpoints && hasExtension(VK_AMD_BUFFER_MARKER_EXTENSION_NAME);
    if (checkpoints || bufferMarkers) {
        deviceExtensions.push_back(checkpoints ? VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME : VK_AMD_BUFFER_MARKER_EXTENSION_NAME);
        deviceInfo.enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size());
        deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
    }
    const bool diagnosticsRequested = std::getenv("APS5_NV_DIAGNOSTICS") != nullptr;
    VkPhysicalDeviceDiagnosticsConfigFeaturesNV diagnosticsFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DIAGNOSTICS_CONFIG_FEATURES_NV};
    if (diagnosticsRequested && hasExtension(VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME)) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &diagnosticsFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
    }
    const auto diagnostics = ChooseDeviceDiagnostics(diagnosticsRequested, diagnosticsFeatures.diagnosticsConfig == VK_TRUE, hasExtension(VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME));
    VkDeviceDiagnosticsConfigCreateInfoNV diagnosticsConfig{VK_STRUCTURE_TYPE_DEVICE_DIAGNOSTICS_CONFIG_CREATE_INFO_NV};
    if (diagnostics.config) {
        diagnosticsFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DIAGNOSTICS_CONFIG_FEATURES_NV};
        diagnosticsFeatures.diagnosticsConfig = VK_TRUE;
        diagnosticsConfig.flags = diagnostics.flags;
        deviceExtensions.push_back(VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME);
        if (diagnostics.checkpoints && !checkpoints) deviceExtensions.push_back(VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME);
        deviceInfo.enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size());
        deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
        diagnosticsConfig.pNext = byteFeatures.pNext;
        diagnosticsFeatures.pNext = &diagnosticsConfig;
        byteFeatures.pNext = &diagnosticsFeatures;
        std::fprintf(stderr, "[gpu] APS5_NV_DIAGNOSTICS: VK_NV_device_diagnostics_config enabled (shader debug info, resource tracking, automatic checkpoints, shader error reporting)\n");
    } else if (diagnosticsRequested) {
        std::fprintf(stderr, "[gpu] APS5_NV_DIAGNOSTICS: the device does not support VK_NV_device_diagnostics_config\n");
    }
    const bool diagnosticCheckpoints = diagnostics.config && diagnostics.checkpoints;
    bdaFeatures.pNext = &byteFeatures;
    VkPhysicalDeviceRobustness2FeaturesEXT robustness2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT};
    if (hasExtension(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME)) {
        VkPhysicalDeviceFeatures2 queried{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &robustness2};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &queried);
    }
    state->robustBufferAccess2 = robustness2.robustBufferAccess2 == VK_TRUE && enabled.robustBufferAccess == VK_TRUE;
    if (state->robustBufferAccess2 && std::getenv("APS5_NO_EXACT_ROBUST_LOADS") == nullptr) {
        VkPhysicalDeviceRobustness2PropertiesEXT robustnessLimits{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_PROPERTIES_EXT};
        VkPhysicalDeviceProperties2 queriedLimits{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &robustnessLimits};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &queriedLimits);
        state->exactRobustLoads = robustnessLimits.robustStorageBufferAccessSizeAlignment == 1u;
    }
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
    if (state->deferDestruction) state->contextDeviceProc = Graphics::DeferDestructionToIdle(state->device, state->deviceProc);
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
        const std::array hosts{VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR};
#else
        const std::array hosts{VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR, VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_KHR};
#endif
        const auto host = std::ranges::find_first_of(hosts, available);
        if (std::ranges::find(available, VK_TIME_DOMAIN_DEVICE_KHR) != available.end() && host != hosts.end()) {
            state->hostTimeDomain = *host;
            state->calibratedTimestamps = khr ? state->DeviceFunction<PFN_vkGetCalibratedTimestampsKHR>("vkGetCalibratedTimestampsKHR") : state->DeviceFunction<PFN_vkGetCalibratedTimestampsKHR>("vkGetCalibratedTimestampsEXT");
        }
    }
    state->DeviceFunction<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(state->device, family, 0, &state->queue);
    state->queueFamily = family;
    if (plan.present) {
        state->DeviceFunction<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(state->device, family, plan.presentIndex, &state->presentQueue);
        APS5_LOG_OUT("Presentation queue family=%u index=%u", family, plan.presentIndex);
    }
    if (settleQueue) {
        state->settleFamily = settleFamily;
        state->DeviceFunction<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(state->device, settleFamily, settleIndex, &state->settleQueue);
    }
    if (state->sparseFamily != VK_QUEUE_FAMILY_IGNORED) state->DeviceFunction<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(state->device, state->sparseFamily, state->sparseIndex, &state->sparseQueue);
    if (checkpoints) state->checkpointMarker = state->DeviceFunction<PFN_vkCmdSetCheckpointNV>("vkCmdSetCheckpointNV");
    if (bufferMarkers) state->CreateMarkerBuffer();
    const bool queueCheckpoints = checkpoints || diagnosticCheckpoints;
    if (deviceFault || queueCheckpoints || bufferMarkers) {
        const auto faultInfo = deviceFault ? state->DeviceFunction<PFN_vkGetDeviceFaultInfoEXT>("vkGetDeviceFaultInfoEXT") : nullptr;
        const auto checkpointData = queueCheckpoints ? state->DeviceFunction<PFN_vkGetQueueCheckpointDataNV>("vkGetQueueCheckpointDataNV") : nullptr;
        Graphics::DeviceLostHook() = [device = state->device, queue = state->queue, present = state->presentQueue, settle = state->settleQueue, sparse = state->sparseQueue, faultInfo, checkpointData, vendorDump = vendorBinary, owner = state.get(), reportResidency = checkpointsRequested] noexcept {
            owner->hookActive.fetch_add(1, std::memory_order_acq_rel);
            try {
                if (!owner->hookRetired.load(std::memory_order_acquire)) {
                    bool stateZero = owner->markerMapping == nullptr;
                    if (owner->markerMapping != nullptr) {
                        std::uint32_t words[2];
                        std::memcpy(words, owner->markerMapping, sizeof(words));
                        stateZero = words[0] == 0 && words[1] == 0;
                        if (stateZero) {
                            std::fprintf(stderr, "[device-fault] no buffer marker reached\n");
                        } else {
                            const AgcDriver::FaultTokenRing& ring = AgcDriver::FaultTokenRing::Instance();
                            for (std::size_t i = 0; i < 2; ++i) {
                                if (words[i] == 0) {
                                    if (i == 1) std::fprintf(stderr, "[device-fault] no dispatch finished\n");
                                    continue;
                                }
                                char tokenLine[192];
                                if (const auto found = ring.Find(static_cast<AgcDriver::FaultToken>(words[i]))) {
                                    std::snprintf(tokenLine, sizeof(tokenLine), "[device-fault] last dispatch %s: token %u, batch %llu, program 0x%llx, variant 0x%llx (%.47s)", i == 0 ? "reached" : "finished", static_cast<unsigned>(found->token), static_cast<unsigned long long>(found->submitSerial), static_cast<unsigned long long>(found->programAddress), static_cast<unsigned long long>(found->variantId), found->desc);
                                } else {
                                    std::snprintf(tokenLine, sizeof(tokenLine), "[device-fault] last dispatch %s: token %u not in the ring", i == 0 ? "reached" : "finished", static_cast<unsigned>(words[i]));
                                }
                                std::fprintf(stderr, "%s\n", tokenLine);
                            }
                        }
                    }
                    if (stateZero) {
                        const std::uint32_t recorderToken = Graphics::LastCheckpointMarkerToken();
                        if (recorderToken != 0) {
                            const AgcDriver::FaultTokenRing& ring = AgcDriver::FaultTokenRing::Instance();
                            char tokenLine[192];
                            if (const auto found = ring.Find(static_cast<AgcDriver::FaultToken>(recorderToken))) {
                                std::snprintf(tokenLine, sizeof(tokenLine), "[device-fault] last dispatch %s: token %u, batch %llu, program 0x%llx, variant 0x%llx (%.47s)", "reached", static_cast<unsigned>(found->token), static_cast<unsigned long long>(found->submitSerial), static_cast<unsigned long long>(found->programAddress), static_cast<unsigned long long>(found->variantId), found->desc);
                            } else {
                                std::snprintf(tokenLine, sizeof(tokenLine), "[device-fault] last dispatch %s: token %u not in the ring", "reached", static_cast<unsigned>(recorderToken));
                            }
                            std::fprintf(stderr, "%s\n", tokenLine);
                        }
                    }
                    std::fprintf(stderr, "[device-fault] gpu %.256s vendor 0x%x device 0x%x driver %.256s commit %.12s%s\n", owner->properties.deviceName, static_cast<unsigned>(owner->properties.vendorID), static_cast<unsigned>(owner->properties.deviceID), owner->driverInfo.c_str(), APS5_TRACE_COMMIT, APS5_TRACE_DIRTY != 0 ? " dirty" : "");
                    {
                        std::uint32_t printedEnvs = 0;
                        const std::uint32_t maxEnvs = 32;
                        std::uint32_t scannedEnvs = 0;
                        const std::uint32_t maxScannedEnvs = 512;
#ifdef _WIN32
                        char** entries = _environ;
#else
                        char** entries = ::environ;
#endif
                        for (; entries != nullptr && *entries != nullptr && printedEnvs < maxEnvs && scannedEnvs < maxScannedEnvs; ++entries) {
                            ++scannedEnvs;
                            const char* entry = *entries;
                            if (std::strncmp(entry, "APS5_", 5) != 0) continue;
                            std::fprintf(stderr, "[device-fault] env %.256s\n", entry);
                            ++printedEnvs;
                        }
                    }
                    const std::uint64_t submitSerial = Graphics::LastCheckpointMarkerSerial();
                    AgcDriver::SubmitBracket bracket{};
                    if (submitSerial != 0) {
                        if (const auto found = AgcDriver::SubmissionSnapshots::DeviceSnapshots().FindBracket(submitSerial)) {
                            bracket = *found;
                        }
                    }
                    std::fprintf(stderr, "[device-fault] queue main\n");
                    if (reportResidency) {
                        const std::uint64_t shadowRefused = Graphics::StagingShadowsRefused();
                        if (shadowRefused != 0) std::fprintf(stderr, "[device-fault] staging shadow copies refused: %llu (process-wide total)\n", static_cast<unsigned long long>(shadowRefused));
                        for (const auto& line : DeviceFaultReport(device, faultInfo, queue, checkpointData, submitSerial, &AgcDriver::SubmissionSnapshots::DeviceSnapshots(), bracket, AgcDriver::FaultTokenRing::Instance(), [](std::uint64_t va) noexcept {
                            AgcDriver::DeviceFaultResidencyProbe probe{};
                            try {
                                if (const auto space = Graphics::GuestBufferMemory::TrySnapshotAddressSpace(); space.has_value() && space->second != nullptr) {
                                    for (const auto& range : *space->second) {
                                        if (range.begin <= va && va < range.end) { probe.boundNow = true; break; }
                                    }
                                }
                            } catch (...) {
                            }
                            return probe;
                        })) std::fprintf(stderr, "%s\n", line.c_str());
                    } else {
                        for (const auto& line : DeviceFaultReport(device, faultInfo, queue, checkpointData, submitSerial, &AgcDriver::SubmissionSnapshots::DeviceSnapshots(), bracket, AgcDriver::FaultTokenRing::Instance())) std::fprintf(stderr, "%s\n", line.c_str());
                    }
                    const VkQueue extraQueues[] = {present, settle, sparse};
                    for (VkQueue extra : extraQueues) {
                        if (extra == VK_NULL_HANDLE || extra == queue) continue;
                        const char* role = extra == present ? "present" : extra == settle ? "settle" : "sparse";
                        std::fprintf(stderr, "[device-fault] queue %s\n", role);
                        for (const auto& line : DeviceFaultReport(VK_NULL_HANDLE, nullptr, extra, checkpointData, 0, nullptr, AgcDriver::SubmitBracket{}, AgcDriver::FaultTokenRing::Instance())) std::fprintf(stderr, "%s\n", line.c_str());
                    }
                    if (vendorDump) DumpDeviceFaultVendorBinary(device, faultInfo, DeviceFaultVendorDumpPath());
                }
            } catch (...) {
            }
            owner->hookActive.fetch_sub(1, std::memory_order_acq_rel);
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
    std::ranges::fill(state->emptyBuffer->Bytes(), std::byte{0});
    state->oversizedBuffer = std::make_unique<Graphics::Buffer>(graphicsContext(), Graphics::EmptyBufferBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    std::ranges::fill(state->oversizedBuffer->Bytes(), std::byte{0});
    if (!compileOnly && Pm4::GdsImportable() && Graphics::HostImportFor(graphicsContext(), Pm4::GdsAddress(), Pm4::GdsBytes, Graphics::ImportRequester::Map) == nullptr) Pm4::RejectGdsImport();
    if (!state->imageInt64Atomics || !state->bufferInt64Atomics) {
        state->atomicLockTable = std::make_unique<Graphics::Buffer>(graphicsContext(), ShaderRecompiler::RuntimeAbi::AtomicLockCount * sizeof(std::uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        const auto words = state->atomicLockTable->Bytes();
        std::memset(words.data(), 0, words.size());
    }
    static const bool headlessPipelineCache = std::getenv("APS5_HEADLESS_PIPELINE_CACHE") != nullptr;
    state->pipelineCache = std::make_unique<Graphics::PipelineCache>(graphicsContext(), state->properties, window != nullptr || compileOnly || headlessPipelineCache);
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
        swapchain.minImageCount = AgcDriver::SwapchainImageCount(surface);
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
        const auto upscale = SelectUpscaleMode(std::getenv("ANYPS5_UPSCALE"), std::getenv("ANYPS5_FSR_SHARPNESS"));
        if (upscale.fsr) {
            state->upscaler = std::make_unique<PresentationUpscaler>(graphicsContext(), state->presentSlots.size(), upscale.sharpness);
            std::fprintf(stderr, "[present] ANYPS5_UPSCALE=fsr: FSR 1 EASU and RCAS when the window is larger than the display buffer, the normal scaler otherwise; RCAS sharpness %.2f\n", static_cast<double>(upscale.sharpness));
        }
    }
    if (state->float32FlushToZeroCandidate) {
        const auto probeStart = std::chrono::steady_clock::now();
        state->probingFloatControls = true;
        const auto probe = ProbeFloat32FlushToZero(*this, Target());
        state->probingFloatControls = false;
        state->float32FlushToZero = probe.mismatches == 0u;
        std::fprintf(stderr, "[float-controls] f32 denormal flush: %s (%u of %u probe results differ from the emulated flush, %.0f ms)\n", state->float32FlushToZero ? "hardware" : "emulated", probe.mismatches, probe.compared, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - probeStart).count());
    }
    if (state->float32DenormPreserve) {
        const auto probeStart = std::chrono::steady_clock::now();
        state->probingFloatControls = true;
        const auto probe = ProbeFloat32DenormPreserve(*this, Target());
        state->probingFloatControls = false;
        state->float32DenormPreserve = probe.mismatches == 0u;
        std::fprintf(stderr, "[float-controls] f32 denormal preserve: %s (%u of %u probe results on normal values differ from the default mode by more than the Vulkan precision bounds, %.0f ms)\n", state->float32DenormPreserve ? "requested" : "not requested", probe.mismatches, probe.compared, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - probeStart).count());
    }
    if (std::getenv("APS5_NO_NATIVE_F16_NANS") == nullptr) {
        const auto probeStart = std::chrono::steady_clock::now();
        state->probingFloatControls = true;
        const auto probe = ProbeFloat16Conversions(*this, Target());
        state->probingFloatControls = false;
        state->float16WidenNans = probe.widen.probed && probe.widen.mismatches == 0u;
        state->float16NarrowNans = probe.narrow.probed && probe.narrow.mismatches == 0u;
        const auto describe = [](const Float16ConversionProbeDirection& direction, bool native) {
            if (!direction.probed) return std::string("emulated (no native conversion)");
            std::string text = std::string(native ? "native" : "emulated") + " (" + std::to_string(direction.mismatches) + " of " + std::to_string(direction.compared) + " probe results differ";
            if (direction.first) {
                char first[96];
                std::snprintf(first, sizeof(first), ", first 0x%08x: 0x%08x emulated, 0x%08x native", direction.first->input, direction.first->emulated, direction.first->native);
                text += first;
            }
            return text + ")";
        };
        std::fprintf(stderr, "[float-controls] f16 to f32 NaN results: %s; f32 to f16 NaN results: %s (%.0f ms)\n", describe(probe.widen, state->float16WidenNans).c_str(), describe(probe.narrow, state->float16NarrowNans).c_str(), std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - probeStart).count());
    }
    if (state->float16HostNanCandidate) {
        const auto probeStart = std::chrono::steady_clock::now();
        state->probingFloatControls = true;
        const auto probe = ProbeFloat16HostNans(*this, Target());
        state->probingFloatControls = false;
        state->float16HostNans = probe.mismatches == 0u;
        std::fprintf(stderr, "[float-controls] f16 NaN results with IEEE_MODE clear: %s (%u of %u probe results differ from the selected NaN, %.0f ms)\n", state->float16HostNans ? "host" : "selected", probe.mismatches, probe.compared, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - probeStart).count());
    }
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
    require(state->computeFullSubgroups, "a spanned wave64 program needs full compute subgroups");
    if (shader.hostSubgroupSize == 0u) return VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT | VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT;
    require(computeSubgroupSize(shader) == 32u, "a spanned wave64 program needs full 32-lane compute subgroups");
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
    const CompileBudget::Hold hold(BackgroundCompileBudget(), shader.spirv.size() * sizeof(std::uint32_t));
    std::promise<std::shared_ptr<ComputePipelineObjects>> building;
    {
        std::lock_guard pipelines(state->computePipelinesMutex);
        if (state->computePipelines.contains(key) || state->computePipelinesBuilding.contains(key)) return false;
        state->computePipelinesBuilding.emplace(key, building.get_future().share());
    }
    std::shared_ptr<ComputePipelineObjects> objects;
    std::exception_ptr error;
    try {
        const auto context = graphicsContext();
        const std::array<Graphics::CompiledShader, 1> shaders{{{ShaderRecompiler::ShaderStage::Compute, &shader, 0}}};
        const auto layouts = Graphics::DescribeSetLayouts(context, shaders);
        objects = createComputePipeline(context, shader, layouts.layouts.main, layouts.mainKey, pushConstants ? VK_SHADER_STAGE_COMPUTE_BIT : 0u, 0);
    } catch (...) {
        error = std::current_exception();
    }
    {
        std::lock_guard pipelines(state->computePipelinesMutex);
        if (objects != nullptr) state->computePipelines.try_emplace(key, objects);
        state->computePipelinesBuilding.erase(key);
    }
    if (error != nullptr) {
        building.set_exception(error);
        std::rethrow_exception(error);
    }
    building.set_value(std::move(objects));
    return true;
}

}
