#include "prx/libc/libc.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <atomic>
#include <bit>
#include <optional>
#include <stdexcept>

namespace {

std::atomic<LegacySigaction_nid_no_patch> signalAction{};

}

extern "C" {

void RegisterSigvec_nid_no_patch(LegacySigaction_nid_no_patch action) {
    signalAction.store(action, std::memory_order_release);
}

int LegacySigvec_nid_no_patch(int signal, const GuestSignalVector* replacement, GuestSignalVector* previous) {
    const auto forward = signalAction.load(std::memory_order_acquire);
    if (forward == nullptr) throw std::runtime_error("sigvec: libkernel has not registered sigaction");
    GuestSigaction action{};
    if (replacement != nullptr) {
        action.handler = replacement->handler;
        action.flags = replacement->flags ^ 2;
        action.mask.bits[0] = static_cast<std::uint32_t>(replacement->mask);
    }
    std::optional<GuestArena::HostWrite> access;
    if (previous != nullptr) {
        access.emplace(previous, sizeof(*previous));
        if (!access->Open()) {
            *__error_nid_postfix() = 14;
            return -1;
        }
    }
    GuestSigaction old{};
    const int result = forward(signal, replacement == nullptr ? nullptr : &action, previous == nullptr ? nullptr : &old);
    if (result == 0 && previous != nullptr) {
        previous->handler = old.handler;
        previous->mask = std::bit_cast<std::int32_t>(old.mask.bits[0]);
        previous->flags = old.flags ^ 2;
    }
    return result;
}

}
