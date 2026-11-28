#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/Socket/include/SocketPoll.hpp"
#include "prx/libkernel/Socket/include/SocketSelectDeadline.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <vector>

namespace {

constexpr int SetMask = 3;
constexpr int Eintr = 4;
constexpr int Efault = 14;

struct Timeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

using SavedSet = std::vector<std::uint64_t>;

bool Save(SavedSet& saved, const void* set, std::size_t bytes) {
    return set == nullptr || GuestCopyIn_nid_no_patch(saved.data(), set, bytes);
}

bool Restore(void* set, const SavedSet& saved, std::size_t bytes) {
    return set == nullptr || GuestCopyOut_nid_no_patch(set, saved.data(), bytes);
}

int Fault() {
    *__error_nid_postfix() = Efault;
    return -1;
}

constexpr std::int64_t SignalSliceMicroseconds = 1000;

int SelectUntilSignal(int nfds, void* readfds, void* writefds, void* exceptfds, const Timeval* wait) {
    const bool validWait = wait == nullptr || (wait->seconds >= 0 && wait->microseconds >= 0 && wait->microseconds < 1000000);
    int scanned = 0;
    int error = 0;
    if (nfds < 0 || !validWait || !KernelSelectCheckSets_nid_no_patch(nfds, readfds, writefds, exceptfds, &scanned, &error))
        return select_nid_postfix(nfds, readfds, writefds, exceptfds, wait);
    if (wait != nullptr && wait->seconds == 0 && wait->microseconds == 0)
        return KernelSelectCheckedSets_nid_no_patch(scanned, readfds, writefds, exceptfds, wait);

    const std::size_t bytes = KernelSocketPoll::SelectSetBytes(scanned);
    const std::size_t words = bytes / sizeof(std::uint64_t) + 1;
    SavedSet read(words), write(words), except(words);
    if (!Save(read, readfds, bytes) || !Save(write, writefds, bytes) || !Save(except, exceptfds, bytes)) return Fault();

    using Clock = std::chrono::steady_clock;
    const auto deadline = wait == nullptr ? Clock::time_point::max()
                                          : KernelSocketSelect::DeadlineAfter(Clock::now(), wait->seconds, wait->microseconds);
    for (;;) {
        if (!Restore(readfds, read, bytes) || !Restore(writefds, write, bytes) || !Restore(exceptfds, except, bytes)) return Fault();
        Timeval slice{0, SignalSliceMicroseconds};
        if (wait != nullptr) {
            const auto now = Clock::now();
            if (now >= deadline) slice.microseconds = 0;
            else {
                const auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(deadline - now).count();
                slice.microseconds = std::min<std::int64_t>(SignalSliceMicroseconds, std::max<std::int64_t>(0, remaining));
            }
        }
        const int result = KernelSelectCheckedSets_nid_no_patch(scanned, readfds, writefds, exceptfds, &slice);
        if (result != 0) return result;
        if (SetSignalMaskCatching_nid_no_patch(nullptr, nullptr) != 0) {
            if (!Restore(readfds, read, bytes) || !Restore(writefds, write, bytes) || !Restore(exceptfds, except, bytes)) return Fault();
            *__error_nid_postfix() = Eintr;
            return -1;
        }
        if (wait != nullptr && Clock::now() >= deadline) return 0;
    }
}
int ScanAfterCaughtSignal(int nfds, void* readfds, void* writefds, void* exceptfds, const Timeval* wait) {
    const bool validWait = wait == nullptr || (wait->seconds >= 0 && wait->microseconds >= 0 && wait->microseconds < 1000000);
    int scanned = 0;
    int error = 0;
    if (nfds < 0 || !validWait || !KernelSelectCheckSets_nid_no_patch(nfds, readfds, writefds, exceptfds, &scanned, &error)) return select_nid_postfix(nfds, readfds, writefds, exceptfds, wait);
    const std::size_t bytes = KernelSocketPoll::SelectSetBytes(scanned);
    const std::size_t words = bytes / sizeof(std::uint64_t) + 1;
    SavedSet read(words), write(words), except(words);
    if (!Save(read, readfds, bytes) || !Save(write, writefds, bytes) || !Save(except, exceptfds, bytes)) return Fault();
    const Timeval zero{};
    const int result = KernelSelectCheckedSets_nid_no_patch(scanned, readfds, writefds, exceptfds, &zero);
    if (result != 0 || (wait != nullptr && wait->seconds == 0 && wait->microseconds == 0)) return result;
    if (!Restore(readfds, read, bytes) || !Restore(writefds, write, bytes) || !Restore(exceptfds, except, bytes)) return Fault();
    *__error_nid_postfix() = Eintr;
    return -1;
}

}

extern "C" {

int APS5_VABI pselect_nid_postfix(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout, const void* sigmask) {
    Timeval limit{};
    const Timeval* wait = nullptr;
    if (timeout != nullptr) {
        std::int64_t spec[2]{};
        if (!GuestCopyIn_nid_no_patch(spec, timeout, sizeof(spec))) return Fault();
        const bool validNsec = spec[1] >= 0 && spec[1] < 1000000000;
        limit = {spec[0], validNsec ? spec[1] / 1000 : -1};
        wait = &limit;
    }
    if (sigmask == nullptr) return select_nid_postfix(nfds, readfds, writefds, exceptfds, wait);
    GuestSignalSet mask{};
    if (!GuestCopyIn_nid_no_patch(&mask, sigmask, sizeof(mask))) return Fault();
    GuestSignalSet previous{};
    const bool caught = SetSignalMaskCatching_nid_no_patch(&mask, &previous) != 0;
    const int result = caught ? ScanAfterCaughtSignal(nfds, readfds, writefds, exceptfds, wait)
                              : SelectUntilSignal(nfds, readfds, writefds, exceptfds, wait);
    const int error = *__error_nid_postfix();
    sigprocmask_nid_postfix(SetMask, &previous, nullptr);
    *__error_nid_postfix() = error;
    return result;
}

}
