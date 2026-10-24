#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {

int* APS5_VABI __error_nid_postfix();
int APS5_VABI select_nid_postfix(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout);
int APS5_VABI sigprocmask_nid_postfix(int how, const void* set, void* previousSet);

int APS5_VABI pselect_nid_postfix(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout, const void* sigmask) {
    constexpr int SetMask = 3;
    struct Timeval {
        std::int64_t seconds;
        std::int64_t microseconds;
    } limit{};
    const Timeval* wait = nullptr;
    if (timeout != nullptr) {
        const auto* spec = static_cast<const std::int64_t*>(timeout);
        limit = {spec[0], spec[1] / 1000};
        wait = &limit;
    }
    std::uint32_t previous[4]{};
    if (sigmask != nullptr && sigprocmask_nid_postfix(SetMask, sigmask, previous) != 0) return -1;
    const int result = select_nid_postfix(nfds, readfds, writefds, exceptfds, wait);
    if (sigmask != nullptr) {
        const int error = *__error_nid_postfix();
        sigprocmask_nid_postfix(SetMask, previous, nullptr);
        *__error_nid_postfix() = error;
    }
    return result;
}

}
