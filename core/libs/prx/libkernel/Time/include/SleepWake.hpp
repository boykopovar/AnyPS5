#ifndef CORE_LIBS_PRX_LIBKERNEL_TIME_INCLUDE_SLEEPWAKE_HPP
#define CORE_LIBS_PRX_LIBKERNEL_TIME_INCLUDE_SLEEPWAKE_HPP

#include <cstdint>

namespace SleepWake {

constexpr std::uint64_t MAX_WAKEABLE_NANOS = 1000000ULL;
constexpr unsigned EARLY_WAKES_PER_RUN = 8;

void Sleep(std::uint64_t nanos);

}

extern "C" {

void KernelWakeSleepers_nid_no_patch();
void KernelNoteGpuSubmit_nid_no_patch();

}

#endif
