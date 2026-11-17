#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_WALKRANGES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_WALKRANGES_HPP

#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace AgcDriver::DriverDetail {

using AddressRanges = std::vector<std::pair<std::uint64_t, std::uint64_t>>;

inline bool RangesSettled(const AddressRanges& ranges, const Graphics::Recorder::WriteRanges* before, std::uint64_t generation) {
    const bool queuedWrite = DrawPipeline::Active() && DrawPipeline::Queue0().Busy() && DrawPipeline::Queue0().OverlapsAny(ranges);
    if (queuedWrite || !Graphics::Recorder::ReadsSettled(ranges, before)) return false;
    for (const auto& [begin, end] : ranges) {
        if (GuestMemory::NewestStamp(begin, static_cast<std::size_t>(end - begin)) > generation) return false;
    }
    return true;
}

inline void GuardRanges(const AddressRanges& ranges) {
    for (const auto& [begin, end] : ranges) GuestMemory::GuardRead(begin, static_cast<std::size_t>(end - begin));
}

}

#endif
