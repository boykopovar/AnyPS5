#include "prx/libSceAgcDriver/Execution/include/AsyncLog.hpp"
#include "prx/libc/include/HostThread.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileCounter.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libc/include/HostThreadSlot.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadScratch.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BdaResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ReferenceStorage.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libkernel/Time/include/SleepWake.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ParallelCompare.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DeferredDestruction.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/SubmissionSnapshots.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/MeshArguments_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/StagingCopyBack_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/StagingCopyBackBound_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/SampleCounter_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/SampleDumps_spv.h"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceOverlay.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4Opcodes.hpp"
#include "prx/libc/include/CpuTopology.hpp"
#include "prx/libc/include/RunReport.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/DispatchArguments_spv.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>

#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <unordered_map>
#include "prx/libc/include/general/AtomicSharedPtr.hpp"

namespace AgcDriver::Graphics {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
void Recorder::removeLabels(Batch& batch) {
    if (!batch.labelDwords.empty() || !batch.wideLabels.empty()) {
        std::lock_guard tableLock(labelTableMutex);
        for (const auto dword : batch.labelDwords) {
            const auto found = labels.find(dword);
            if (found != labels.end() && found->second.batch == &batch) labels.erase(found);
        }
        for (const auto entry : batch.wideLabels) wideLabels.erase(entry);
        recordedLabels.store(labels.size(), std::memory_order_relaxed);
        recordedWideLabels.store(wideLabels.size(), std::memory_order_relaxed);
    }
    batch.labelDwords.clear();
    batch.wideLabels.clear();
}

void Recorder::release(Batch& batch) noexcept {
    if (batch.queries != VK_NULL_HANDLE) {
        if (!GpuTimingEnabled() && sparePools.size() < 64) {
            try {
                sparePools.push_back(batch.queries);
                batch.queries = VK_NULL_HANDLE;
            } catch (...) {
            }
        }
        if (batch.queries != VK_NULL_HANDLE) context.Function<PFN_vkDestroyQueryPool>("vkDestroyQueryPool")(context.device, batch.queries, nullptr);
    }
    batch.queries = VK_NULL_HANDLE;
    if (batch.samples != VK_NULL_HANDLE && batch.samplePool == nullptr) context.Function<PFN_vkDestroyQueryPool>("vkDestroyQueryPool")(context.device, batch.samples, nullptr);
    batch.samples = VK_NULL_HANDLE;
    batch.samplePool.reset();
    for (const auto pool : batch.endedSamples) context.Function<PFN_vkDestroyQueryPool>("vkDestroyQueryPool")(context.device, pool, nullptr);
    batch.endedSamples.clear();
    batch.segments = VK_NULL_HANDLE;
    batch.segmentPool.reset();
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (batch.commands != VK_NULL_HANDLE) {
        context.Function<PFN_vkFreeCommandBuffers>("vkFreeCommandBuffers")(context.device, context.pool, 1, &batch.commands);
        if (profile) commandBuffersFreed.fetch_add(1, std::memory_order_relaxed);
    }
    batch.commands = VK_NULL_HANDLE;
    if (batch.fence != VK_NULL_HANDLE && spare.size() < 64 && function(resetFences, "vkResetFences")(context.device, 1, &batch.fence) == VK_SUCCESS) {
        try {
            spare.push_back(batch.fence);
            batch.fence = VK_NULL_HANDLE;
            if (profile) fencesKept.fetch_add(1, std::memory_order_relaxed);
            return;
        } catch (const std::bad_alloc&) {
        }
    }
    if (batch.fence != VK_NULL_HANDLE) context.Function<PFN_vkDestroyFence>("vkDestroyFence")(context.device, batch.fence, nullptr);
    batch.fence = VK_NULL_HANDLE;
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
