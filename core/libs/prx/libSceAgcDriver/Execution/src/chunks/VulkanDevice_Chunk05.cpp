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
bool VulkanDevice::present(std::uint32_t width, std::uint32_t height, bool opaque, std::span<const std::byte> pixels, const DisplayBuffer* display, const std::shared_ptr<Graphics::StorageTexture>& resident, VkFilter residentFilter, bool dumpFrame, bool residentConvert, const VkClearColorValue* uniform, std::uint32_t residentLayer, bool passthrough, const DisplayBuffer* plane) {
    PerformanceTimer timing("Vulkan.Present");
    APS5_LOG_OUT_DEBUG("present begin width=%u height=%u opaque=%u pixels=%zu", width, height, static_cast<unsigned>(opaque), pixels.size());
    require(state->swapchain != VK_NULL_HANDLE, "device has no swapchain");
    require(state->extent.width != 0 && state->extent.height != 0, "output window is minimized");
    require(!state->queuePending, "the previous presentation was not queued");
    require(!state->presentSlots.empty(), "device has no presentation slots");
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
    const bool shared = !clearOnly && !direct && !drawn;
    const bool upscaled = state->upscaler != nullptr && !clearOnly && PresentationUpscaler::Upscales(width, height, state->extent);
    if (shared && state->SharedSlotInFlight()) RetirePresents(0);
    if (state->presentSlots[state->presentCursor].inFlight) RetirePresents(state->presentSlots.size() - 1);
    auto& slot = state->presentSlots[state->presentCursor];
    require(!slot.inFlight, "presentation slot is still in flight");
    const bool overlay = PerformanceOverlay::Visible();
    if (plane != nullptr && state->surfaceColorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT) {
        static std::once_flag reported;
        std::call_once(reported, [] { std::fprintf(stderr, "[present] the overlay plane is not composited over an HDR10 presentation\n"); });
        plane = nullptr;
    }
    const auto planeSource = plane != nullptr ? OverlayPlaneSource(graphicsContext(), *plane) : PlaneSource{};
    const auto systemLayout = SystemOverlayLayout(state->extent);
    const bool keyboard = systemLayout.has_value();
    const auto padStatusLines = PadStatusOverlayLines();
    const bool padStatusShown = PadStatusDrawn(padStatusLines.has_value(), keyboard);
    const bool drawsOver = overlay || keyboard || padStatusShown || plane != nullptr;
    if (display != nullptr && resident == nullptr) {
        static_cast<void>(DisplayBufferSize(*display));
        state->colorTransfer->Upload(display->address, width, height, Graphics::ColorTileMode::RenderTarget, DisplayTexelBytes(display->pixelFormat));
    }
    if (!pixels.empty()) state->Upload(pixels);
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
    const auto planeSlot = static_cast<std::size_t>(&slot - state->presentSlots.data());
    VkImageMemoryBarrier planeBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    bool planeBound = false;
    const auto bindPlane = [&] {
        if (planeBound) return;
        planeBound = true;
        if (!state->planePass) state->planePass = std::make_unique<PresentationPass>(graphicsContext(), state->presentSlots.size());
        if (planeSource.resident == nullptr) {
            state->planePass->BindTexels(planeSlot, commands, planeSource.texels, plane->width, plane->height, plane->pixelFormat, whiteNits);
            return;
        }
        planeBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        planeBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        planeBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        planeBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        planeBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        planeBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        planeBarrier.image = planeSource.resident->Image();
        planeBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS};
        pipelineBarrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &planeBarrier);
        Graphics::Recorder::CountBarriers(CommandClass::PresentBlit);
        state->planePass->Bind(planeSlot, planeSource.resident->Image(), plane->width, plane->height, plane->pixelFormat, whiteNits, planeSource.layer);
    };
    const auto blendPlaneInto = [&](VkImage image, VkFormat format, VkExtent2D extent) {
        if (plane == nullptr) return;
        bindPlane();
        state->planePass->RecordOverlay(commands, planeSlot, image, format, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, extent, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    };
    if (!drawn && !upscaled) {
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
        if (upscaled) {
            const auto stageExtent = PresentationUpscaler::StageExtent(width, height, state->extent);
            const auto stage = state->upscaler->Stage(slotIndex, state->surfaceFormat, stageExtent);
            if (drawn) {
                state->presentation->Bind(slotIndex, resident->Image(), width, height, display->pixelFormat, whiteNits, residentLayer);
                state->presentation->RecordStage(commands, slotIndex, stage, state->surfaceFormat, state->surfaceColorSpace, stageExtent, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            } else {
                const AspectFitRect whole{0, 0, stageExtent.width, stageExtent.height};
                state->upscaler->RecordStageTransferDestination(commands, slotIndex);
                if (direct) PresentationScaler::RecordStretchFrom(graphicsContext(), commands, resident->Image(), VK_IMAGE_LAYOUT_GENERAL, width, height, residentFilter, stage, whole, residentLayer);
                else scaler->RecordStretch(commands, stage, whole);
                state->upscaler->RecordStageWritten(commands, slotIndex, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
            }
            state->upscaler->RecordPresent(commands, slotIndex, width, height, barrier.image, state->extent, drawsOver ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        } else if (drawn) {
            state->presentation->Bind(slotIndex, resident->Image(), width, height, display->pixelFormat, whiteNits, residentLayer);
            state->presentation->RecordPresent(commands, slotIndex, barrier.image, state->surfaceFormat, state->surfaceColorSpace, state->extent, drawsOver ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
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
                state->presentation->RecordDumpImage(commands, slotIndex, scale);
                blendPlaneInto(state->presentation->DumpImage(slotIndex), VK_FORMAT_B8G8R8A8_UNORM, state->presentation->DumpImageExtent(slotIndex));
                state->presentation->RecordDumpReadback(commands, slotIndex, slot.dumpBuffer->Handle());
            } else if (direct) {
                if (!slot.dumpScaler) slot.dumpScaler = std::make_unique<PresentationScaler>(graphicsContext(), VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM);
                slot.dumpScaler->EnsureSourceImage(slot.dumpWidth, slot.dumpHeight);
                slot.dumpScaler->RecordBlitInto(commands, resident->Image(), VK_IMAGE_LAYOUT_GENERAL, VK_FILTER_NEAREST, width, height, residentLayer);
                blendPlaneInto(slot.dumpScaler->SourceImage(), VK_FORMAT_B8G8R8A8_UNORM, {slot.dumpWidth, slot.dumpHeight});
                slot.dumpScaler->RecordReadback(commands, slot.dumpBuffer->Handle());
            } else {
                blendPlaneInto(scaler->SourceImage(), scaler->SourceFormat(), {scaler->SourceWidth(), scaler->SourceHeight()});
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
    if (plane != nullptr) {
        bindPlane();
        state->planePass->RecordOverlay(commands, planeSlot, barrier.image, state->surfaceFormat, state->surfaceColorSpace, state->extent, drawn || upscaled ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, overlay || keyboard || padStatusShown ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        if (planeSource.resident != nullptr) {
            planeBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            planeBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            pipelineBarrier(commands, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &planeBarrier);
            Graphics::Recorder::CountBarriers(CommandClass::PresentBlit);
        }
    }
    if (keyboard) {
        if (!state->keyboard) state->keyboard = std::make_unique<PerformanceOverlay>(graphicsContext(), state->presentSlots.size());
        state->keyboard->RecordRects(commands, static_cast<std::size_t>(&slot - state->presentSlots.data()), barrier.image, state->surfaceFormat, state->surfaceColorSpace, state->extent, drawn || upscaled || plane != nullptr ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, overlay ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, *systemLayout);
    }
    if (padStatusShown) {
        if (!state->padStatus) state->padStatus = std::make_unique<PerformanceOverlay>(graphicsContext(), state->presentSlots.size());
        state->padStatus->RecordRects(commands, static_cast<std::size_t>(&slot - state->presentSlots.data()), barrier.image, state->surfaceFormat, state->surfaceColorSpace, state->extent, drawn || upscaled || plane != nullptr ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, overlay ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, LayOutPadStatus(*padStatusLines, state->extent, PerformanceOverlay::Corner()));
    }
    if (overlay) {
        if (!state->overlay) state->overlay = std::make_unique<PerformanceOverlay>(graphicsContext(), state->presentSlots.size());
        state->overlay->NotePresent(std::chrono::steady_clock::now(), clearOnly ? VkExtent2D{} : VkExtent2D{width, height}, OverlayPresentInfo{std::exchange(state->presentedAddress, 0), state->presentMode, presentCounters.acquireDrops, ConfiguredFrameLimit() ? ConfiguredFrameLimit()->FramesPerSecond() : 0.0});
        state->overlay->Record(commands, static_cast<std::size_t>(&slot - state->presentSlots.data()), barrier.image, state->surfaceFormat, state->surfaceColorSpace, state->extent, drawn || upscaled || plane != nullptr || keyboard || padStatusShown ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    } else if (!drawn && !upscaled && plane == nullptr && !keyboard && !padStatusShown) {
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
    check(state->DeviceFunction<PFN_vkResetFences>("vkResetFences")(state->device, 1, &slot.fence), "vkResetFences present");
    check(state->DeviceFunction<PFN_vkQueueSubmit>("vkQueueSubmit")(state->queue, 1, &submit, slot.fence), "vkQueueSubmit clear");
    const auto submittedAt = std::chrono::steady_clock::now();
    timing.Mark("queue_submit");
    APS5_LOG_CHARS_OUT_DEBUG("Presentation vkQueueSubmit OK");
    {
        std::lock_guard lock(state->presentMutex);
        slot.kept = resident;
        slot.keptPlane = planeSource.resident;
        slot.imageIndex = index;
        slot.inFlight = true;
        slot.shared = shared;
        slot.dumpRecorded = dumpFrame;
        slot.dumpScreenshot = dumpFrame && state->nextDumpScreenshot;
        slot.dumpIndex = state->nextDumpIndex;
        slot.recorderSerial = batchesAtBlit;
        slot.previousSerial = state->lastPresentSerial;
        slot.submittedAt = submittedAt;
        state->lastPresentSerial = batchesAtBlit;
        state->presentCursor = (state->presentCursor + 1) % state->presentSlots.size();
        state->queuePending = true;
        state->queueGuestFrame = state->presentingGuestFrame;
        state->queueIndex = index;
        ++presentCounters.presents;
        if (upscaled) upscaledPresents.fetch_add(1, std::memory_order_relaxed);
        if (plane != nullptr) ++presentCounters.planes;
        if (auto* frame = PerformanceContext::Current()) {
            frame->NoteBlit(batchesAtBlit, lastSubmittedAt, submittedAt);
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

std::uint64_t VulkanDevice::UpscaledPresents() {
    return upscaledPresents.load(std::memory_order_relaxed);
}

std::uint64_t VulkanDevice::FillCompletionSyncCount() { return fillCompletionSyncs; }

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

}
}
}
}
}
}
