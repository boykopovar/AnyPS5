#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_PACKETS_DRIVERTIMED_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_PACKETS_DRIVERTIMED_HPP

#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Packets/PacketTiming.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceOverlay.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadScratch.hpp"
#include <chrono>
#include <cstdint>
#include <cstdlib>

namespace AgcDriver::DriverDetail {

template <typename TWork>
void Driver::timed(double WorkerProfile::*bucket, TWork&& work) {
    static const bool report = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!report) {
        work();
        return;
    }
    struct Slot {};
    auto& profile = ThreadScratch<WorkerProfile, Slot>();
    const auto begin = std::chrono::steady_clock::now();
    work();
    const auto end = std::chrono::steady_clock::now();
    profile.*bucket += std::chrono::duration<double, std::milli>(end - begin).count();
    if (end - profile.reported > std::chrono::seconds(10)) {
        profile.reported = end;
        AgcDriver::ProfilePrint_nid_no_patch( "[gpu] worker at %.0f s: dispatch %.1f s, draw %.1f s, wait %.1f s\n", std::chrono::duration<double>(end - profile.start).count(), profile.dispatchMs / 1000, profile.drawMs / 1000, profile.waitMs / 1000);
    }
    if (report && bucket != &WorkerProfile::waitMs) PerformanceCounters::AddDriverNanoseconds(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()));
}

}

#endif
