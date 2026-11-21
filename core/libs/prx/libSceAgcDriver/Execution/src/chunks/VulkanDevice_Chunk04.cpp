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

bool VulkanDevice::PresentClear(std::uint32_t width, std::uint32_t height, bool opaque, const DisplayBuffer* plane) {
    state->presentingGuestFrame = false;
    APS5_LOG_OUT_DEBUG("PresentClear width=%u height=%u opaque=%u", width, height, static_cast<unsigned>(opaque));
    return present(width, height, opaque, {}, nullptr, nullptr, VK_FILTER_LINEAR, false, false, nullptr, 0, false, plane);
}

void VulkanDevice::PresentPixels(std::uint32_t width, std::uint32_t height, std::span<const std::byte> pixels) {
    APS5_LOG_OUT("PresentPixels width=%u height=%u pixels=%zu expected=%llu", width, height, pixels.size(), static_cast<unsigned long long>(width) * height * 4u);
    require(width != 0 && height != 0 && width <= 16384 && height <= 16384, "invalid display image extent");
    require(pixels.size() == static_cast<std::uint64_t>(width) * height * 4, "invalid display pixel buffer size");
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
    state->presentingGuestFrame = false;
    if (!present(width, height, true, pixels)) return;
    FinishPresent();
    QueuePresent();
}

namespace {

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

void QueueFrameDump(FrameDumpWriter& writer, int index, std::uint32_t fullWidth, std::uint32_t fullHeight, std::span<const std::byte> full) {
    const auto scale = DumpScale();
    require(full.size() == static_cast<std::size_t>(fullWidth) * fullHeight * 4, "invalid frame dump pixels");
    const std::uint32_t width = (fullWidth + scale - 1) / scale;
    const std::uint32_t height = (fullHeight + scale - 1) / scale;
    std::vector<std::byte> pixels(static_cast<std::size_t>(width) * height * 4);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) std::memcpy(pixels.data() + (static_cast<std::size_t>(y) * width + x) * 4, full.data() + (static_cast<std::size_t>(y) * scale * fullWidth + x * scale) * 4, 4);
    }
    writer.Enqueue(index, width, height, false, std::move(pixels));
}

void WriteFrameBmp(int index, std::uint32_t width, std::uint32_t height, std::span<const std::byte> pixels) {
    if (!frameDumpOutput.Enabled_nid_no_patch()) return;
    char name[32];
    std::snprintf(name, sizeof(name), "frame_%03d.bmp", index);
    frameDumpOutput.Write_nid_no_patch(name, width, height, pixels);
}

ResidentPresent ResidentPresentable(const Graphics::Context& context, const Graphics::StorageTexture& image, const DisplayBuffer& buffer, VkFilter& filter, std::uint32_t& layer) {
    const auto& descriptor = image.Descriptor();
    const bool gcn = buffer.tilingMode == DisplayTilingGcn;
    if (gcn && (descriptor.tileMode != Graphics::TextureTileMode::kGcnDisplay2D || descriptor.width != buffer.pitchInPixel || descriptor.height < buffer.height || descriptor.mipCount != 1 || image.ImageDepth() != 1)) return ResidentPresent::None;
    if (!gcn && (descriptor.width != buffer.width || descriptor.height != buffer.height || descriptor.mipCount != 1 || image.ImageDepth() != 1)) return ResidentPresent::None;
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

bool NoResidentPresent() {
    static const bool no = std::getenv("APS5_NO_RESIDENT_PRESENT") != nullptr;
    return no;
}

bool RefreshPresented(Graphics::StorageTexture& resident, const DisplayBuffer& buffer, std::uint32_t layer) {
    static const bool noAdopt = std::getenv("APS5_NO_PRESENT_EPOCH") != nullptr;
    const auto bytes = DisplayBufferSize(buffer);
    if (!noAdopt) GuestMemory::AdoptLatestCollectEpoch();
    GuestMemory::CollectWritesUncached(buffer.address, bytes);
    if (GuestMemory::UnchangedSince(buffer.address, bytes, resident.GenerationFor(buffer.address, bytes)) && (buffer.dccAddress == 0 || !ResidentKeysMoved(resident))) return false;
    if (resident.ImageLayers() > 1) resident.RefreshLayers(layer, 1);
    else resident.Refresh();
    return true;
}

struct PlaneSource {
    std::shared_ptr<Graphics::StorageTexture> resident;
    std::uint32_t layer = 0;
    std::vector<std::byte> texels;
};

PlaneSource OverlayPlaneSource(const Graphics::Context& context, const DisplayBuffer& buffer) {
    require(DisplayTexelBytes(buffer.pixelFormat) == 4u && !DisplayTenBit(buffer.pixelFormat) && !DisplayPq(buffer.pixelFormat), "VideoOut: only an 8-bit SDR overlay plane is composited over the main output");
    PlaneSource source;
    if (!NoResidentPresent()) {
        VkFilter filter = VK_FILTER_LINEAR;
        bool pending = false;
        bool convert = false;
        source.resident = PresentableResident(context, buffer, filter, pending, convert, source.layer);
        if (source.resident != nullptr && buffer.dccAddress != 0 && !ResidentServesDisplay(*source.resident, buffer)) source.resident.reset();
        if (source.resident != nullptr) {
            static_cast<void>(RefreshPresented(*source.resident, buffer, source.layer));
            return source;
        }
    }
    const auto bytes = DisplayBufferSize(buffer);
    GuestMemory::FlushGpuWrites(buffer.address, bytes);
    const auto keys = buffer.dccAddress != 0 ? DisplayBufferKeys(buffer) : Graphics::DccKeys::Uncompressed;
    if (keys == Graphics::DccKeys::Uncompressed) {
        GuestMemory::CheckRange(reinterpret_cast<const void*>(buffer.address), bytes, buffer.alignment);
        source.texels = DisplayBufferTexels(buffer, {reinterpret_cast<const std::byte*>(buffer.address), bytes});
        return source;
    }
    const auto texel = DisplayBufferClearTexel(buffer, keys);
    source.texels.resize(static_cast<std::size_t>(buffer.width) * buffer.height * texel.size());
    for (std::size_t offset = 0; offset < source.texels.size(); offset += texel.size()) std::memcpy(source.texels.data() + offset, texel.data(), texel.size());
    return source;
}

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

bool VulkanDevice::PresentDisplayBuffer(const DisplayBuffer& buffer, const DisplayBuffer* plane) {
    state->presentingGuestFrame = true;
    struct ClearGuestFrame {
        bool& flag;
        ~ClearGuestFrame() { flag = false; }
    } clearGuestFrame{state->presentingGuestFrame};
    state->presentedAddress = buffer.address;
    if (buffer.dccAddress != 0 && buffer.tilingMode == 0) {
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        const auto before = profile ? Graphics::Recorder::ThreadWaitedMs() : 0.0;
        state->recorder->SyncThrough(buffer.dccAddress, DisplayBufferKeyBytes(buffer), true);
        if (const auto waited = profile ? Graphics::Recorder::ThreadWaitedMs() - before : 0.0; waited > 0.0) {
            ++presentCounters.keyWriterWaits;
            presentCounters.keyWriterWaitMs += waited;
        }
    }
    static const bool storeAtFlip = Graphics::StorageTexture::StoreAtFlipRequested(std::getenv("APS5_STORE_AT_FLIP"));
    if (storeAtFlip) Graphics::StorageTexture::FlushAllPending("flip", state->device);
    else Graphics::StorageTexture::FlushDumpSelected("flip");
    auto& dumps = Dumps();
    const bool passthrough = state->surfaceColorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT;
    require(!passthrough || DisplayPq(buffer.pixelFormat), "only a PQ display buffer is presented to the HDR10 swapchain");
    if (buffer.tilingMode == 1) {
        if (passthrough) ensureHdrScaler(DisplayTexelFormat(buffer.pixelFormat));
        const auto pixels = passthrough ? ReadDisplayBufferTexels(buffer) : ReadDisplayBuffer(buffer);
        bool dumpFrame = false;
        std::optional<int> cpuDump;
        if (frameDumpOutput.Enabled_nid_no_patch() && dumps.dumped < dumps.limit && ++dumps.presents % static_cast<std::uint64_t>(dumps.every) == 0) {
            if (passthrough || dumps.cpu) {
                cpuDump = dumps.dumped++;
            } else {
                state->nextDumpIndex = dumps.dumped++;
                dumpFrame = true;
            }
        }
        const bool screenshot = takeScreenshot(dumpFrame);
        if (cpuDump && passthrough) QueueFrameDump(state->dumpWriter, *cpuDump, buffer.width, buffer.height, ReadDisplayBuffer(buffer));
        else if (cpuDump) QueueFrameDump(state->dumpWriter, *cpuDump, buffer.width, buffer.height, pixels);
        auto shot = screenshot && passthrough ? ReadDisplayBuffer(buffer) : std::vector<std::byte>{};
        if (!present(buffer.width, buffer.height, true, pixels, nullptr, nullptr, VK_FILTER_LINEAR, dumpFrame || (screenshot && !passthrough), false, nullptr, 0, passthrough, plane)) {
            if (dumpFrame) --dumps.dumped;
            if (screenshot) Screenshot::Request();
            return false;
        }
        if (!shot.empty()) state->dumpWriter.Enqueue(0, buffer.width, buffer.height, true, std::move(shot));
        return true;
    }
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
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
            if (RefreshPresented(*resident, buffer, layer)) ++refreshedPresents;
            ++residentPresents;
        }
    }
    const auto cleared = buffer.dccAddress != 0 && resident == nullptr ? CompressedClearPixel(buffer, bytes) : std::nullopt;
    bool dumpFrame = false;
    std::optional<int> cpuDump;
    if (frameDumpOutput.Enabled_nid_no_patch() && dumps.dumped < dumps.limit && ++dumps.presents % static_cast<std::uint64_t>(dumps.every) == 0) {
        if (dumps.cpu || (passthrough && resident == nullptr)) {
            cpuDump = dumps.dumped++;
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
    if (cpuDump) QueueFrameDump(state->dumpWriter, *cpuDump, buffer.width, buffer.height, DisplayPixels(buffer, cleared));
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
    const auto decoded = buffer.tilingMode != DisplayTilingGcn || resident != nullptr ? std::vector<std::byte>{} : passthrough ? ReadDisplayBufferTexels(buffer) : ReadDisplayBuffer(buffer);
    if (!present(buffer.width, buffer.height, true, decoded, cleared || !decoded.empty() ? nullptr : &buffer, resident, filter, dumpFrame || (screenshot && !cpuScreenshot), convert, cleared ? &uniform : nullptr, layer, passthrough, plane)) {
        if (dumpFrame) --dumps.dumped;
        if (screenshot) Screenshot::Request();
        return false;
    }
    if (cpuScreenshot) state->dumpWriter.Enqueue(0, buffer.width, buffer.height, true, std::move(shot));
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
    const auto acquired = state->DeviceFunction<PFN_vkAcquireNextImageKHR>("vkAcquireNextImageKHR")(state->device, state->swapchain, AgcDriver::SwapchainAcquireTimeoutNs, VK_NULL_HANDLE, state->acquireFence, &index);
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

}
}
}
}
}
