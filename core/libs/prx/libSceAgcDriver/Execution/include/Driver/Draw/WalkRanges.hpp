#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_WALKRANGES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_WALKRANGES_HPP

#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace AgcDriver::DriverDetail {

using AddressRanges = std::vector<std::pair<std::uint64_t, std::uint64_t>>;

inline std::atomic<std::uint64_t> PreciseSettles{0};

inline bool RangesSettled(const AddressRanges& ranges, const Graphics::Recorder::WriteRanges* before, std::uint64_t generation, const Graphics::Recorder::WriteRanges* also = nullptr) {
    const bool queuedWrite = DrawPipeline::Active() && DrawPipeline::Queue0().Busy() && DrawPipeline::Queue0().OverlapsAny(ranges);
    if (queuedWrite || !Graphics::Recorder::ReadsSettled(ranges, before, also)) return false;
    if (Graphics::RuntimeSwitches::Set(Graphics::RuntimeSwitch::FastSettle) && GuestMemory::TrackerGenerationNow() <= generation) return true;
    const bool precise = Graphics::RuntimeSwitches::Set(Graphics::RuntimeSwitch::PreciseSettle);
    for (const auto& [begin, end] : ranges) {
        const auto bytes = static_cast<std::size_t>(end - begin);
        if (GuestMemory::StampsAtMost(begin, bytes, generation)) continue;
        if (!precise || !GuestMemory::UnchangedSince(begin, bytes, generation)) return false;
        PreciseSettles.fetch_add(1, std::memory_order_relaxed);
    }
    return true;
}

inline void GuardRanges(const AddressRanges& ranges) {
    for (const auto& [begin, end] : ranges) GuestMemory::GuardRead(begin, static_cast<std::size_t>(end - begin));
}

}

#endif
