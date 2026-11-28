#include "prx/libkernel/include/Api.hpp"
#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

namespace {

constexpr std::uint32_t MicrosecondsPerSecond = 1000000;
constexpr int ItimerReal = 0;

}

std::uint32_t APS5_VABI ualarm_nid_postfix(std::uint32_t usecs, std::uint32_t reload) {
    GuestItimerval next{};
    next.interval.tv_sec = reload / MicrosecondsPerSecond;
    next.interval.tv_usec = reload % MicrosecondsPerSecond;
    next.value.tv_sec = usecs / MicrosecondsPerSecond;
    next.value.tv_usec = usecs % MicrosecondsPerSecond;
    GuestItimerval previous{};
    if (setitimer_nid_postfix(ItimerReal, &next, &previous) == 0)
        return static_cast<std::uint32_t>(previous.value.tv_sec * MicrosecondsPerSecond + previous.value.tv_usec);
    return static_cast<std::uint32_t>(-1);
}
