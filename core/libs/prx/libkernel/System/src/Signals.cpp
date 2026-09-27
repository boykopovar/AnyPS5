#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <mutex>

extern "C" int* APS5_VABI __error_nid_postfix();
namespace {
using GuestHandler = void (APS5_VABI *)(int);
std::atomic<GuestHandler> handlers[32]{};
static_assert(std::atomic<GuestHandler>::is_always_lock_free);
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
    const auto callback = handlers[guest].load();
    if (reinterpret_cast<std::uintptr_t>(callback) > 1) callback(guest);
}
}
extern "C" {
// FreeBSD's guest sigset_t is four 32-bit words, covering signals 1..128.
struct GuestSignalSet { std::uint32_t bits[4]; };
static_assert(sizeof(GuestSignalSet) == 16);

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
int APS5_VABI raise_nid_postfix(int guest) {
    const int native = NativeSignal(guest);
    if (!native) { *__error_nid_postfix() = 22; return -1; }
    const int result = std::raise(native);
    if (result) *__error_nid_postfix() = 22;
    return result ? -1 : 0;
}
}
