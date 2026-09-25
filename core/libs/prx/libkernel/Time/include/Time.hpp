#ifndef CORE_LIBS_PRX_LIBKERNEL_TIME_INCLUDE_TIME_HPP
#define CORE_LIBS_PRX_LIBKERNEL_TIME_INCLUDE_TIME_HPP

#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {

    std::uint64_t APS5_VABI sceKernelGetProcessTime();
    std::uint64_t APS5_VABI sceKernelGetProcessTimeCounter();
    std::uint64_t APS5_VABI sceKernelGetProcessTimeCounterFrequency();
    int APS5_VABI sceKernelUsleep_nid_postfix(KernelUseconds microseconds);
    int APS5_VABI sceKernelNanosleep(const KernelTimespec* rqtp, KernelTimespec* rmtp);
    int APS5_VABI nanosleep_nid_postfix(const KernelTimespec* rqtp, KernelTimespec* rmtp);
    int APS5_VABI clock_gettime_nid_postfix(int clockId, KernelTimespec* tp);
    int APS5_VABI clock_getres_nid_postfix(int clockId, KernelTimespec* res);
    int APS5_VABI gettimeofday_nid_postfix(KernelTimeval* tv, KernelTimezone* tz);

    // Debug aid: APS5_TRACE_WAITS=1 aggregates blocking waits (event flags, semaphores, condition
    // variables, event queues) per guest call site and prints the sites that wait longest every 3 s,
    // with their timeout rate; a site that times out once per frame is a lost wake-up.
    void KernelTraceWait_nid_postfix(const char* kind, const void* caller, std::uint64_t waitedNanos, bool timedOut);

}

#endif
