#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_PACKETTIMING_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_PACKETTIMING_HPP

#include "prx/libSceAgcDriver/Execution/include/Driver/Dispatch/DispatchTiming.hpp"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <utility>

namespace AgcDriver::DriverDetail {

struct WorkerProfile {
    double dispatchMs = 0;
    double drawMs = 0;
    double waitMs = 0;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point reported = start;
};

struct PacketProfile {
    std::map<std::uint32_t, std::pair<std::uint64_t, double>> byOpcode;
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
    std::uint64_t submissions = 0;

    double flushMs = 0;

    std::array<std::pair<std::uint64_t, double>, static_cast<std::size_t>(DispatchOutcome::Count)> dispatchOutcomes{};
};

struct DrainEpochs {
    static constexpr std::size_t Flip = 256;
    std::uint64_t interrupts = 0, queuedInterrupts = 0;
    std::array<std::uint64_t, Flip + 1> packets{};
};

DrainEpochs& DrainEpochCounts();

struct PacketTimer {
    bool enabled; std::uint32_t key; std::uint32_t queue; PacketProfile& profile; std::chrono::steady_clock::time_point start;
    ~PacketTimer();
};

}

#endif
