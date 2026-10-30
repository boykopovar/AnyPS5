#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <csignal>
#include <cstdint>
#include <cstdlib>

namespace {

constexpr int Unix = 1;
constexpr int Stream = 1;
constexpr int BrokenPipe = 13;
constexpr int PipeError = 32;
constexpr int TryAgain = 35;
volatile std::sig_atomic_t brokenPipeSignals = 0;

void Require(bool value) {
    if (!value) std::abort();
}

void APS5_VABI CountBrokenPipe(int signal) {
    if (signal == BrokenPipe) brokenPipeSignals = brokenPipeSignals + 1;
}

int WriteToClosedPeer() {
    int pair[2] = {-1, -1};
    Require(socketpair_nid_postfix(Unix, Stream, 0, pair) == 0);
    Require(close_nid_postfix(pair[1]) == 0);
    *__error_nid_postfix() = 0;
    const bool failed = write_nid_postfix(pair[0], "x", 1) == -1;
    const int error = *__error_nid_postfix();
    Require(close_nid_postfix(pair[0]) == 0);
    return failed ? error : 0;
}

}

int main() {
    const auto ignore = reinterpret_cast<GuestHandler>(std::uintptr_t{1});
    Require(signal_nid_postfix(BrokenPipe, ignore) == nullptr);
    Require(WriteToClosedPeer() == PipeError);
    Require(signal_nid_postfix(BrokenPipe, CountBrokenPipe) == ignore);
    Require(WriteToClosedPeer() == PipeError && brokenPipeSignals == 1);
    GuestSignalSet blocked{};
    blocked.bits[0] = 1u << (BrokenPipe - 1);
    Require(_sigprocmask_nid_postfix(1, &blocked, nullptr) == 0);
    Require(WriteToClosedPeer() == PipeError && brokenPipeSignals == 1);
    GuestSignalSet pending{};
    Require(sigpending_nid_postfix(&pending) == 0 && (pending.bits[0] & blocked.bits[0]) != 0);
    Require(_sigprocmask_nid_postfix(2, &blocked, nullptr) == 0 && brokenPipeSignals == 2);
    const KernelTimespec poll{0, 0};
    Require(_sigprocmask_nid_postfix(1, &blocked, nullptr) == 0);
    Require(WriteToClosedPeer() == PipeError);
    Require(sigtimedwait_nid_postfix(&blocked, nullptr, &poll) == BrokenPipe);
    Require(signal_nid_postfix(BrokenPipe, ignore) == CountBrokenPipe);
    Require(WriteToClosedPeer() == PipeError);
    *__error_nid_postfix() = 0;
    Require(sigtimedwait_nid_postfix(&blocked, nullptr, &poll) == -1 && *__error_nid_postfix() == TryAgain);
    Require(_sigprocmask_nid_postfix(2, &blocked, nullptr) == 0 && brokenPipeSignals == 2);
    Require(signal_nid_postfix(BrokenPipe, nullptr) == ignore);
    return 0;
}
