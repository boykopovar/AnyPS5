#include "prx/libc/include/HostThreadSlot.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Execution/include/AsyncLog.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SlimMutex.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/AtomicSharedPtr.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Execution/include/DeviceFault.hpp"
#include "prx/libc/include/HostThreadSlot.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadScratch.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MapImportGate.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
// For the declaration of PendingStorageOverlaps, defined below.
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "ShaderCacheDirectory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/UnitShadow.hpp"
#include "prx/libSceAgcDriver/Graphics/include/KeptUnits.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "RdnaDecoder/include/RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "IntermediateRepresentation/include/IntermediateRepresentation/IrMetadata/BufferFormat.hpp"
#include <algorithm>
#include <filesystem>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <stdexcept>
#include <vector>
#include <atomic>
#include <mutex>
#include <map>
#include <limits>
#include <exception>
#include <functional>
#include <optional>
#include <thread>
#include <unordered_map>
#include <utility>
#if defined(__APPLE__)
#include <unistd.h>
#endif

namespace AgcDriver::Graphics {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
StorageTexture::~StorageTexture() {
    {
        auto& live = Live();
        std::lock_guard lock(live.mutex);
        std::erase(live.textures, this);
    }
    // Cache eviction flushes first; anything still pending here is being torn down with the device.
    {
        auto& pending = Pending();
        std::lock_guard lock(pending.mutex);
        forgetDrawn();
        if (dirty) {
            dirty = false;
            pending.textures.remove(this);
            BumpPendingSerial();
            static std::atomic<int> reports{0};
            if (reports.fetch_add(1) < 4) std::fprintf(stderr, "[gpu] storage image 0x%llx destroyed with GPU results pending\n", static_cast<unsigned long long>(descriptor.baseAddress));
        }
    }
    release();
}

void StorageTexture::release() noexcept {
    for (const auto& [mip, extra] : extraViews) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, extra, nullptr);
    extraViews.clear();
    for (const auto& [mip, extra] : firstLayerViews) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, extra, nullptr);
    firstLayerViews.clear();
    for (const auto& [key, atomic] : atomicViews) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, atomic, nullptr);
    atomicViews.clear();
    for (const auto& [key, uint] : uintViews) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, uint, nullptr);
    uintViews.clear();
    if (elementView) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, elementView, nullptr);
    elementView = VK_NULL_HANDLE;
    for (const auto& [key, element] : elementLayerViews) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, element, nullptr);
    elementLayerViews.clear();
    for (const auto& [format, attachment] : attachmentViews) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, attachment, nullptr);
    attachmentViews.clear();
    if (proxyView) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, proxyView, nullptr);
    if (proxyImage) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, proxyImage, nullptr);
    proxyAllocation.Reset();
    if (view) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, view, nullptr);
    if (image) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
    imageAllocation.Reset();
}

std::uint64_t StorageTexture::GuestBytes() const {
    return guestBytes;
}

std::uint64_t StorageTexture::BudgetBytes() const {
    return std::max<std::uint64_t>(AllocationBytes(), GuestBytes());
}

VkImageView StorageTexture::View() const {
    return view;
}

}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
