#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstdlib>

extern "C" int APS5_VABI sceKernelSyncOnAddressWait(std::uint32_t* address, std::uint32_t expected, const KernelUseconds* timeout, const char* name);
extern "C" int APS5_VABI sceKernelSyncOnAddressWake(void* address, std::int32_t count);

namespace {

constexpr std::uintptr_t OnceIdle = 0;
constexpr std::uintptr_t OnceRunning = 1;
constexpr std::uintptr_t OnceDone = 2;
constexpr unsigned long InitFailed = 4;

std::atomic<std::uintptr_t>& OnceState(void** once) {
    return *reinterpret_cast<std::atomic<std::uintptr_t>*>(once);
}

}

extern "C" {

int APS5_VABI __std_init_once_begin_initialize_clr_nid_postfix(void** once, unsigned long flags, int* pending, void** context) {
    (void)flags;
    (void)context;
    auto& state = OnceState(once);
    for (;;) {
        std::uintptr_t current = state.load(std::memory_order_acquire);
        if (current == OnceIdle) {
            if (state.compare_exchange_weak(current, OnceRunning, std::memory_order_acq_rel)) {
                *pending = 1;
                return 1;
            }
            continue;
        }
        if (current == OnceDone) {
            *pending = 0;
            return 1;
        }
        sceKernelSyncOnAddressWait(reinterpret_cast<std::uint32_t*>(once), static_cast<std::uint32_t>(OnceRunning), nullptr, nullptr);
    }
}

int APS5_VABI __std_init_once_complete_clr_nid_postfix(void** once, unsigned long flags, void* context) {
    (void)context;
    OnceState(once).store(flags == InitFailed ? OnceIdle : OnceDone, std::memory_order_release);
    sceKernelSyncOnAddressWake(once, INT_MAX);
    return 1;
}

[[noreturn]] void APS5_VABI __std_init_once_link_alternate_names_and_abort_nid_postfix() {
    std::abort();
}

}
