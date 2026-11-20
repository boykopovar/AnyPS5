#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DeferredDestruction.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestWorkMark.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/RunReport.hpp"
#include <algorithm>
#include <array>
#include <exception>
#ifdef __APPLE__
#include <cstring>
#endif

namespace AgcDriver::Graphics {
namespace {
void CommandBatch::Wait() {
    Require(submitted, "command batch has not been submitted");
    if (!pending) return;
    PerformanceTimer timing("Graphics.Wait");
    const auto result = context.Function<PFN_vkWaitForFences>("vkWaitForFences")(context.device, 1, &fence, VK_TRUE, 5'000'000'000ULL);
    timing.Mark("fence_wait");
    if (result == VK_SUCCESS || result == VK_ERROR_DEVICE_LOST) pending = false;
    Check(result, "vkWaitForFences graphics");
    // Recorded batches preceded this one, so their completions (write-backs) can run now.
    if (auto* recorder = Recorder::Active()) recorder->Reap();
}

}
}
