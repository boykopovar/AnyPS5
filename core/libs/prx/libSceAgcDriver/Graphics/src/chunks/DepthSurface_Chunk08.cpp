#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/HostThreadSlot.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/PrimedSharedMutex.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/FirstSampleCopy.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuardSpans.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTextureLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/DepthToStorage_spv.h"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "RdnaDecoder/include/RdnaDecoder/RdnaDescriptorFormat.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <limits>
#include <shared_mutex>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
void StoreDepthExpandWrites(const Context& context) {
    std::vector<std::pair<std::uint64_t, std::size_t>> planes;
    {
        std::lock_guard lock(surfacesMutex());
        auto& unstored = unstoredExpands();
        for (auto entry = unstored.begin(); entry != unstored.end();) {
            if (entry->first.first != context.device) {
                ++entry;
                continue;
            }
            planes.emplace_back(entry->first.second, entry->second);
            entry = unstored.erase(entry);
        }
    }
    std::vector<std::pair<std::uint64_t, std::size_t>> kept;
    for (const auto& [address, bytes] : planes) {
        if (!HostImportOverlaps(context, address, bytes)) {
            const std::array<std::pair<std::uint64_t, std::uint64_t>, 1> range{{{address, address + bytes}}};
            if (StorageTexture::AnyPendingOverlaps(range)) kept.emplace_back(address, bytes);
            continue;
        }
        if (StorageTexture::FlushPending(address, bytes, nullptr, "DB expand")) expandsStored.fetch_add(1, std::memory_order_relaxed);
    }
    if (kept.empty()) return;
    std::lock_guard lock(surfacesMutex());
    for (const auto& [address, bytes] : kept) unstoredExpands().try_emplace({context.device, address}, bytes);
}

DepthExpandStatistics DepthExpandCounts() {
    return {expandsRun.load(std::memory_order_relaxed), expandsSkipped.load(std::memory_order_relaxed), expandsStored.load(std::memory_order_relaxed)};
}

void RetireCoveredDepthSurfaces(VkDevice device, std::span<const std::pair<std::uint64_t, std::uint64_t>> written, std::span<const WrittenStorageExtent> extents) {
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::lock_guard lock(surfacesMutex());
    for (const auto& surface : surfaces()) {
        if (surface->context.device != device || surface->retired || !surface->CoveredBy(written, extents)) continue;
        if (!surface->PlaneCoveredBy(true, written, extents)) storeStaleStencil(*surface);
        retire(*surface);
        if (surface->target.samples == 1u || !surface->samplingActive || surface->hostWritten->load(std::memory_order_acquire)) continue;
        std::unique_lock guardLock(depthGuardMutex());
        surface->active->store(false, std::memory_order_release);
        std::erase_if(depthGuards(), [&](const auto& range) { return range.active == surface->active; });
        anyDepthGuard().store(!depthGuards().empty(), std::memory_order_release);
    }
}

bool DirectDepthCopy(const Context& context, VkFormat depthFormat, VkImageAspectFlagBits aspect, VkFormat colorFormat) {
    if (!context.maintenance8 || !directDepthCopy.load(std::memory_order_relaxed)) return false;
    const auto bytes = aspectCopyBytes(depthFormat, aspect);
    return bytes != 0 && singleComponent(colorFormat, bytes);
}

std::uint64_t DirectDepthCopyCount() {
    return directDepthCopies.load(std::memory_order_relaxed);
}

void SetDirectDepthCopy(bool enabled) {
    directDepthCopy.store(enabled && std::getenv("APS5_NO_DIRECT_DEPTH_COPY") == nullptr, std::memory_order_relaxed);
}

void SetDepthTransferPasses(bool enabled) {
    depthTransferPasses.store(enabled && std::getenv("APS5_NO_DEPTH_TRANSFER_PASS") == nullptr, std::memory_order_relaxed);
}

std::uint64_t DepthTransferPassCount() {
    return depthTransferPassCount.load(std::memory_order_relaxed);
}

bool DepthUploadImage(const Context& context) {
    return context.depthUploadImage || forcedDepthUploadImage.load(std::memory_order_relaxed);
}

std::uint64_t DepthUploadCount() {
    return depthUploads.load(std::memory_order_relaxed);
}

void SetDepthUploadImage(bool enabled) {
    forcedDepthUploadImage.store(enabled && std::getenv("APS5_NO_DEPTH_UPLOAD_IMAGE") == nullptr, std::memory_order_relaxed);
}

}
}
}
}
}
}
}
