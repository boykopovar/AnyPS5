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

namespace AgcDriver {
namespace {
namespace {
namespace {
namespace {
namespace {
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
    std::unique_lock presentQueueLock(state->presentQueueMutex, std::defer_lock);
    if (state->presentQueue != VK_NULL_HANDLE) presentQueueLock.lock();
    const auto presented = state->DeviceFunction<PFN_vkQueuePresentKHR>("vkQueuePresentKHR")(state->presentQueue != VK_NULL_HANDLE ? state->presentQueue : state->queue, &present);
    if (presented == VK_ERROR_OUT_OF_DATE_KHR) state->extent = {0, 0};
    else if (presented != VK_SUBOPTIMAL_KHR) check(presented, "vkQueuePresentKHR");
    if (state->queueGuestFrame && (presented == VK_SUCCESS || presented == VK_SUBOPTIMAL_KHR)) LibcRunReportFramePresented_nid_no_patch();
    state->queueGuestFrame = false;
    timing.Mark("queue_present");
    APS5_LOG_OUT_DEBUG("vkQueuePresentKHR queued imageIndex=%u", index);
}

bool VulkanDevice::PresentsOnOwnQueue() const {
    return state->presentQueue != VK_NULL_HANDLE;
}

ShaderRecompiler::SpirvTarget VulkanDevice::ComputeTarget(std::uint32_t waveSize) const {
    auto target = Target();
    target.float32FlushToZero = state->float32FlushToZero;
    if (waveSize == 32u && state->computeWave32) target.subgroupSize = 32u;
    return target;
}

std::string VulkanDevice::DeviceName() const {
    return state->properties.deviceName;
}

std::string VulkanDevice::DriverInfo() const {
    return state->driverInfo.empty() ? std::to_string(state->properties.driverVersion) : state->driverInfo;
}

VkDriverId VulkanDevice::DriverId() const {
    return state->driverId;
}

bool VulkanDevice::BdaAtomicsUsable() const {
    return AgcDriver::BdaAtomicsUsable(state->driverId);
}

bool VulkanDevice::RasterOrderGroupUsable() const {
    return AgcDriver::RasterOrderGroupUsable(state->driverId);
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
    clock_gettime(state->hostTimeDomain == VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_KHR ? CLOCK_MONOTONIC_RAW : CLOCK_MONOTONIC, &monotonic);
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
    target.float32FlushToZero = state->float32FlushToZero && !state->float32FlushComputeOnly;
    target.float16HostNans = state->float16HostNans;
    target.fusedExactArithmetic = state->fusedExactArithmetic;
    target.hostInterpolationFirst = state->hostInterpolationFirst;
    target.float32DenormPreserve = state->float32DenormPreserve;
    target.float16WidenNans = state->float16WidenNans;
    target.float16NarrowNans = state->float16NarrowNans;
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
    target.fragmentBarycentricBuiltins = state->fragmentBarycentricBuiltins;
    target.nonConstantImageOffsets = state->maintenance8;
    target.shaderImageMinLod = state->shaderImageMinLod;
    target.narrowSubgroupClock = state->narrowSubgroupClock;
    target.float64DenormPreserve = state->float64DenormPreserve;
    target.float16DenormPreserve = state->float16DenormPreserve;
    target.spannedWaves = state->spannedWaves;
    target.spannedAnySubgroup = state->spannedAnySubgroup;
    target.depthRangeRemap = !state->depthRangeUnrestricted;
    target.fixedPushSlots = state->graphicsPipelineLibrary;
    target.registrationModules = state->graphicsPipelineLibrary;
    target.robustBufferAccess2 = state->robustBufferAccess2;
    target.exactRobustLoads = state->exactRobustLoads;
    target.waveLdsMemoryBarriers = !state->subgroupControlBarriers;
    target.largePixelFunctionLdsBuffer = state->driverId == VK_DRIVER_ID_NVIDIA_PROPRIETARY;
    target.pixelFunctionStorageDwords = state->pixelFunctionStorageDwords;
    target.deviceScopeLdsMemoryBarriers = !state->workgroupScopeFences;
    target.ldsLockFences = state->ldsLockFences;
    target.deviceAtomicWorkgroupMemoryBarriers = state->deviceAtomicWorkgroupMemoryBarriers;
    target.meshWindowBuffer = state->driverId == VK_DRIVER_ID_NVIDIA_PROPRIETARY && std::getenv("APS5_NO_MESH_WINDOW_BUFFER") == nullptr;
    target.meshInlineFunctions = state->driverId == VK_DRIVER_ID_NVIDIA_PROPRIETARY && std::getenv("APS5_NO_MESH_INLINE_FUNCTIONS") == nullptr;
    target.srgbDecodeFormats = state->srgbDecodeFormats & ~state->srgbCopyFormats;
    target.maxImageArrayLayers = limits.maxImageArrayLayers;
    target.vertexSubgroups = (state->subgroup.supportedStages & VK_SHADER_STAGE_VERTEX_BIT) != 0;
    target.tessellationEvaluationSubgroups = (state->subgroup.supportedStages & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) != 0;
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

bool VulkanDevice::SamplerMirrorClampToEdge() const {
    return state->samplerMirrorClampToEdge;
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

std::pair<VkBuffer, std::uint64_t> VulkanDevice::scratchPoolHeader(const ShaderRecompiler::RecompileResult& shader) const {
    std::lock_guard lock(state->scratchPoolsMutex);
    const auto found = state->scratchPools.find(shader.Identity());
    if (found == state->scratchPools.end() || found->second == nullptr) return {VK_NULL_HANDLE, 0};
    return {found->second->buffer->Handle(), found->second->headerBytes};
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
        state->contextDeviceProc != nullptr ? state->contextDeviceProc : state->deviceProc,
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
        state->fragmentBarycentricBuiltins,
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
    context.hostImportsInHeapUsage = Graphics::HostImportsInHeapUsage(state->driverId);
    context.tessellationPipelineCache = state->tessellationPipelineCache;
    context.hostPointerImport = state->hostPointerImport;
    context.dmaBufImport = state->dmaBufImport;
    context.sparseBinding = state->sparseBinding;
    context.sparseResidencyBuffer = state->sparseResidencyBuffer;
    context.sparseResidencyAliased = state->sparseResidencyAliased;
    context.sparseResidencyImage2D = state->sparseResidencyImage2D;
    context.sparseImageFormatProperties = state->sparseResidencyImage2D ? state->InstanceFunction<PFN_vkGetPhysicalDeviceSparseImageFormatProperties>("vkGetPhysicalDeviceSparseImageFormatProperties") : nullptr;
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
    context.vertexFormats = &state->vertexFormats;
    context.samplerCache = state->samplerCache.get();
    context.drawIndirectFirstInstance = state->drawIndirectFirstInstance;
    context.multiDrawIndirect = state->multiDrawIndirect;
    context.dualSrcBlend = state->dualSrcBlend;
    context.logicOp = state->logicOp;
    context.depthBounds = state->depthBounds;
    context.depthBiasClamp = state->depthBiasClamp;
    context.samplerFilterMinmax = state->samplerFilterMinmax;
    context.samplerMirrorClampToEdge = state->samplerMirrorClampToEdge;
    context.nonSeamlessCubeMap = state->nonSeamlessCubeMap;
    context.mergedCoverageSampleCounts = state->mergedCoverageSampleCounts;
    context.truncatedCoverageSampleCounts = state->truncatedCoverageSampleCounts;
    context.mixedAttachmentSamples = state->mixedAttachmentSamples;
    context.conservativeRasterization = state->conservativeRasterization;
    context.meshWave32 = state->meshWave32;
    context.provokingVertexLast = state->provokingVertexLast;
    context.graphicsPipelineLibrary = state->graphicsPipelineLibrary;
    context.pipelineLinkTimeOptimization = state->pipelineLinkTimeOptimization;
    context.dynamicColorWriteMask = state->dynamicColorWriteMask;
    context.drawInputsInPlace = state->drawInputsInPlace;
    context.drawInputsInPlaceBelow = state->drawInputsInPlaceBelow;
    context.discreteGpu = state->properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    context.integratedGpu = state->properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
    context.provokingVertexModePerPipeline = state->provokingVertexModePerPipeline;
    context.drawIndirectCount = state->drawIndirectCount;
    context.occlusionQueryPrecise = state->occlusionQueryPrecise;
    context.emptyBuffer = state->emptyBuffer ? state->emptyBuffer->Handle() : VK_NULL_HANDLE;
    context.oversizedBuffer = state->oversizedBuffer ? state->oversizedBuffer->Handle() : VK_NULL_HANDLE;
    context.exactRobustLoads = state->exactRobustLoads;
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
    context.multiViewport = state->multiViewport;
    context.largePoints = state->largePoints;
    context.sampleRateShading = state->sampleRateShading;
    context.fillModeNonSolid = state->fillModeNonSolid;
    context.wideLines = state->wideLines;
    context.maintenance5 = state->maintenance5;
    context.storageImageMultisample = state->storageImageMultisample;
    context.storageImageWriteWithoutFormat = state->storageImageWriteWithoutFormat;
    context.shaderClipDistance = state->shaderClipDistance;
    context.shaderCullDistance = state->shaderCullDistance;
    context.shaderTerminateInvocation = state->shaderTerminateInvocation;
    context.nullDescriptors = state->shaderProfile != nullptr && state->shaderProfile->NullDescriptors();
    context.bufferInt64Atomics = state->bufferInt64Atomics;
    context.atomicLockTable = state->atomicLockTable ? state->atomicLockTable->DeviceAddress() : 0;
    context.placeholderImages = state->placeholderImages.get();
    context.viewportIndexLayer = state->viewportIndexLayer;
    context.imageFormatList = state->imageFormatList;
    context.demoteToHelperInvocation = state->demoteToHelperInvocation;
    context.earlyFragmentSampleMaskTestBeforeSampleCounting = state->earlyFragmentSampleMaskTestBeforeSampleCounting;
    context.fmaFloat16 = state->fmaFloat16;
    context.primitiveListRestart = state->primitiveListRestart;
    context.imageViewMinLod = state->imageViewMinLod;
    context.shaderResourceMinLod = state->shaderResourceMinLod;
    context.shaderImageMinLod = state->shaderImageMinLod;
    context.pipelineExecutableInfo = state->pipelineExecutableInfo;
    context.maintenance8 = state->maintenance8;
    context.pushDescriptors = state->pushDescriptors;
    context.depthUploadImage = state->depthUploadImage;
    context.srgbDecodeFormats = state->srgbDecodeFormats;
    context.srgbCopyFormats = state->srgbCopyFormats;
    context.scaledSampledFormats = state->scaledSampledFormats;
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
    return {state->drawIndirectFirstInstance, state->multiDrawIndirect, state->drawIndirectCount, state->indexedIndirectPatches};
}

std::optional<std::string> VulkanDevice::KnownDrawRejection(const Graphics::State& graphics, std::span<const Graphics::CompiledShader> shaders) const {
    return Graphics::KnownValidationFailure(graphicsContext(), shaders, graphics);
}

void VulkanDevice::ColorMetadataPass(const Graphics::ColorMetadataPass& pass) {
    Graphics::RunColorMetadataPass(graphicsContext(), pass);
}

void VulkanDevice::DepthExpandPass(const Graphics::DepthExpandPass& pass) {
    Graphics::RunDepthExpandPass(graphicsContext(), pass);
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

}
}
}
}
}
}
