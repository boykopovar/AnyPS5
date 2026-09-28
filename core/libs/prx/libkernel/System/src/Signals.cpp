#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

extern "C" int* APS5_VABI __error_nid_postfix();
namespace {
using GuestHandler = void (APS5_VABI *)(int);
std::atomic<GuestHandler> handlers[32]{};
static_assert(std::atomic<GuestHandler>::is_always_lock_free);
std::atomic<std::uint32_t> blockedMask{0};
static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
std::mutex registration;
int NativeSignal(int guest) {
    switch (guest) {
        case 2: return SIGINT;
        case 4: return SIGILL;
        case 6: return SIGABRT;
        case 8: return SIGFPE;
        case 11: return SIGSEGV;
        case 15: return SIGTERM;
        default: return 0;
    }
}
void Dispatch(int native) {
    int guest = 0;
    for (int candidate : {2, 4, 6, 8, 11, 15})
        if (NativeSignal(candidate) == native) { guest = candidate; break; }
    if (!guest) return;
#ifdef _WIN32
    // Preserve the guest's persistent registration across CRT delivery.
    std::signal(native, Dispatch);
#endif
    if ((blockedMask.load() & (1u << guest)) != 0) return;
    const auto callback = handlers[guest].load();
    if (reinterpret_cast<std::uintptr_t>(callback) > 1) callback(guest);
}
}

extern "C" {
// FreeBSD's guest sigset_t is four 32-bit words, covering signals 1..128.
struct GuestSignalSet { std::uint32_t bits[4]; };
static_assert(sizeof(GuestSignalSet) == 16);
struct GuestSignalAction {
    GuestHandler handler;
    int flags;
    GuestSignalSet mask;
};
static_assert(offsetof(GuestSignalAction, flags) == 8 &&
              offsetof(GuestSignalAction, mask) == 12 && sizeof(GuestSignalAction) == 32);
static thread_local GuestSignalSet guestThreadMask{};

int APS5_VABI pthread_sigmask_nid_postfix(int how, const GuestSignalSet* set, GuestSignalSet* oldSet) {
    if (set != nullptr && how != 1 && how != 2 && how != 3) return 22;
    const GuestSignalSet previous = guestThreadMask;
    if (set != nullptr) {
        for (unsigned index = 0; index < 4; ++index) {
            switch (how) {
                case 1: guestThreadMask.bits[index] |= set->bits[index]; break;
                case 2: guestThreadMask.bits[index] &= ~set->bits[index]; break;
                case 3: guestThreadMask.bits[index] = set->bits[index]; break;
            }
        }
        // SIGKILL (9) and SIGSTOP (17) cannot be blocked by the guest.
        guestThreadMask.bits[0] &= ~((std::uint32_t{1} << 8) | (std::uint32_t{1} << 16));
    }
    if (oldSet != nullptr) *oldSet = previous;
    return 0;
}

int APS5_VABI sigprocmask_nid_postfix(int how, const GuestSignalSet* set, GuestSignalSet* oldSet) {
    const int result = pthread_sigmask_nid_postfix(how, set, oldSet);
    if (result != 0) { *__error_nid_postfix() = result; return -1; }
    return 0;
}

int APS5_VABI sigemptyset_nid_postfix(GuestSignalSet* set) {
    if (set == nullptr) { *__error_nid_postfix() = 22; return -1; }
    std::memset(set, 0, sizeof(*set));
    return 0;
}

int APS5_VABI sigfillset_nid_postfix(GuestSignalSet* set) {
    if (set == nullptr) { *__error_nid_postfix() = 22; return -1; }
    std::memset(set, 0xff, sizeof(*set));
    return 0;
}

int APS5_VABI sigaddset_nid_postfix(GuestSignalSet* set, int signal) {
    if (set == nullptr || signal < 1 || signal > 128) { *__error_nid_postfix() = 22; return -1; }
    const auto bit = static_cast<unsigned>(signal - 1);
    set->bits[bit / 32] |= std::uint32_t{1} << (bit % 32);
    return 0;
}

int APS5_VABI sigdelset_nid_postfix(GuestSignalSet* set, int signal) {
    if (set == nullptr || signal < 1 || signal > 128) { *__error_nid_postfix() = 22; return -1; }
    const auto bit = static_cast<unsigned>(signal - 1);
    set->bits[bit / 32] &= ~(std::uint32_t{1} << (bit % 32));
    return 0;
}

int APS5_VABI sigismember_nid_postfix(const GuestSignalSet* set, int signal) {
    if (set == nullptr || signal < 1 || signal > 128) { *__error_nid_postfix() = 22; return -1; }
    const auto bit = static_cast<unsigned>(signal - 1);
    return (set->bits[bit / 32] & (std::uint32_t{1} << (bit % 32))) != 0;
}

int APS5_VABI sigisemptyset_nid_postfix(const GuestSignalSet* set) {
    if (set == nullptr) { *__error_nid_postfix() = 22; return -1; }
    for (const auto word : set->bits) if (word != 0) return 0;
    return 1;
}

int APS5_VABI sigandset_nid_postfix(GuestSignalSet* result, const GuestSignalSet* left, const GuestSignalSet* right) {
    if (result == nullptr || left == nullptr || right == nullptr) { *__error_nid_postfix() = 22; return -1; }
    for (unsigned index = 0; index < 4; ++index) result->bits[index] = left->bits[index] & right->bits[index];
    return 0;
}

int APS5_VABI sigorset_nid_postfix(GuestSignalSet* result, const GuestSignalSet* left, const GuestSignalSet* right) {
    if (result == nullptr || left == nullptr || right == nullptr) { *__error_nid_postfix() = 22; return -1; }
    for (unsigned index = 0; index < 4; ++index) result->bits[index] = left->bits[index] | right->bits[index];
    return 0;
}

GuestHandler APS5_VABI signal_nid_postfix(int guest, GuestHandler handler) {
    const auto invalid = reinterpret_cast<GuestHandler>(static_cast<std::uintptr_t>(-1));
    const int native = NativeSignal(guest);
    if (!native || handler == invalid) { *__error_nid_postfix() = 22; return invalid; }
    std::lock_guard lock(registration);
    const auto previous = handlers[guest].exchange(handler);
    const auto address = reinterpret_cast<std::uintptr_t>(handler);
    auto hostHandler = address == 0 ? SIG_DFL : address == 1 ? SIG_IGN : Dispatch;
    if (std::signal(native, hostHandler) == SIG_ERR) {
        handlers[guest].store(previous);
        *__error_nid_postfix() = 22;
        return invalid;
    }
    return previous;
}
int APS5_VABI sigaction_nid_postfix(int guest, const GuestSignalAction* action,
                                   GuestSignalAction* previous) {
    if (!NativeSignal(guest)) { *__error_nid_postfix() = 22; return -1; }
    if (action) {
        // The host signal bridge currently supports only the basic handler
        // and ignore/default dispositions; it cannot honor sa_flags or mask.
        if (action->flags != 0) { *__error_nid_postfix() = 45; return -1; }
        for (const auto word : action->mask.bits)
            if (word != 0) { *__error_nid_postfix() = 45; return -1; }
        if (reinterpret_cast<std::uintptr_t>(action->handler) == static_cast<std::uintptr_t>(-1)) {
            *__error_nid_postfix() = 22;
            return -1;
        }
    }
    GuestSignalAction old{};
    if (action) {
        const auto prior = signal_nid_postfix(guest, action->handler);
        if (reinterpret_cast<std::uintptr_t>(prior) == static_cast<std::uintptr_t>(-1)) return -1;
        old.handler = prior;
    } else {
        std::lock_guard lock(registration);
        old.handler = handlers[guest].load();
    }
    if (previous) *previous = old;
    return 0;
}
int APS5_VABI raise_nid_postfix(int guest) {
    const int native = NativeSignal(guest);
    if (!native) { *__error_nid_postfix() = 22; return -1; }
    const int result = std::raise(native);
    if (result) *__error_nid_postfix() = 22;
    return result ? -1 : 0;
}
int APS5_VABI _sigprocmask_nid_postfix(int how, const GuestSignalSet* set, GuestSignalSet* previousSet) {
    std::lock_guard lock(registration);
    if (previousSet != nullptr) {
        previousSet->bits[0] = blockedMask.load();
        previousSet->bits[1] = 0;
        previousSet->bits[2] = 0;
        previousSet->bits[3] = 0;
    }
    if (set != nullptr) {
        switch (how) {
            case 1: blockedMask.fetch_or(set->bits[0]); break;
            case 2: blockedMask.fetch_and(~set->bits[0]); break;
            case 3: blockedMask.store(set->bits[0]); break;
            default: throw std::invalid_argument("_sigprocmask: invalid how");
        }
    }
    return 0;
}
}

extern "C" {

int APS5_VABI _is_signal_return_nid_postfix(std::uint64_t programCounter) {
    (void)programCounter;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
