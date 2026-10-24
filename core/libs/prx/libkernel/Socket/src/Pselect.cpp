#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstring>

extern "C" {

int* APS5_VABI __error_nid_postfix();
int APS5_VABI select_nid_postfix(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout);
int APS5_VABI sigprocmask_nid_postfix(int how, const void* set, void* previousSet);
int APS5_VABI SetSignalMaskCatching_nid_no_patch(const void* set, void* previousSet);

}

namespace {

constexpr int SetMask = 3;
constexpr int Eintr = 4;
constexpr int DescriptorLimit = 1024;

struct Timeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

using SavedSet = std::array<std::uint64_t, DescriptorLimit / 64>;

void Save(SavedSet& saved, const void* set, std::size_t bytes) {
    if (set != nullptr) std::memcpy(saved.data(), set, bytes);
}

void Restore(void* set, const SavedSet& saved, std::size_t bytes) {
    if (set != nullptr) std::memcpy(set, saved.data(), bytes);
}

int ScanAfterCaughtSignal(int nfds, void* readfds, void* writefds, void* exceptfds, const Timeval* wait) {
    const bool validWait = wait == nullptr || (wait->seconds >= 0 && wait->microseconds >= 0 && wait->microseconds < 1000000);
    if (nfds < 0 || nfds > DescriptorLimit || !validWait) return select_nid_postfix(nfds, readfds, writefds, exceptfds, wait);
    const std::size_t bytes = static_cast<std::size_t>((nfds + 63) / 64) * sizeof(std::uint64_t);
    SavedSet read{}, write{}, except{};
    Save(read, readfds, bytes);
    Save(write, writefds, bytes);
    Save(except, exceptfds, bytes);
    const Timeval zero{};
    const int result = select_nid_postfix(nfds, readfds, writefds, exceptfds, &zero);
    if (result != 0 || (wait != nullptr && wait->seconds == 0 && wait->microseconds == 0)) return result;
    Restore(readfds, read, bytes);
    Restore(writefds, write, bytes);
    Restore(exceptfds, except, bytes);
    *__error_nid_postfix() = Eintr;
    return -1;
}

}

extern "C" {

int APS5_VABI pselect_nid_postfix(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout, const void* sigmask) {
    Timeval limit{};
    const Timeval* wait = nullptr;
    if (timeout != nullptr) {
        const auto* spec = static_cast<const std::int64_t*>(timeout);
        limit = {spec[0], spec[1] / 1000};
        wait = &limit;
    }
    if (sigmask == nullptr) return select_nid_postfix(nfds, readfds, writefds, exceptfds, wait);
    std::uint32_t previous[4]{};
    const bool caught = SetSignalMaskCatching_nid_no_patch(sigmask, previous) != 0;
    const int result = caught ? ScanAfterCaughtSignal(nfds, readfds, writefds, exceptfds, wait)
                              : select_nid_postfix(nfds, readfds, writefds, exceptfds, wait);
    const int error = *__error_nid_postfix();
    sigprocmask_nid_postfix(SetMask, previous, nullptr);
    *__error_nid_postfix() = error;
    return result;
}

}
