#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

extern "C" {
int APS5_VABI __std_init_once_begin_initialize_clr_nid_postfix(void** once, unsigned long flags, int* pending, void** context);
int APS5_VABI __std_init_once_complete_clr_nid_postfix(void** once, unsigned long flags, void* context);
}

namespace {

constexpr unsigned long InitFailed = 4;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "init once: %s\n", message);
    std::abort();
}

int Begin(void** once) {
    int pending = -1;
    Require(__std_init_once_begin_initialize_clr_nid_postfix(once, 0, &pending, nullptr) == 1, "begin returns 1");
    return pending;
}

}

int main() {
    alignas(8) void* once = nullptr;
    Require(Begin(&once) == 1, "the first caller runs the initialization");
    Require(reinterpret_cast<std::uintptr_t>(once) == 1, "a running initialization marks the object");
    Require(__std_init_once_complete_clr_nid_postfix(&once, InitFailed, nullptr) == 1, "complete a failed initialization");
    Require(once == nullptr, "a failed initialization resets the object");
    Require(Begin(&once) == 1, "a caller after a failure runs the initialization again");

    std::atomic<bool> waiterDone{false};
    std::atomic<int> waiterPending{-1};
    std::thread waiter([&] {
        waiterPending = Begin(&once);
        waiterDone = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Require(!waiterDone, "a second caller waits while the initialization runs");
    Require(__std_init_once_complete_clr_nid_postfix(&once, 0, nullptr) == 1, "complete the initialization");
    waiter.join();
    Require(waiterPending == 0, "the waiting caller sees the initialization done");
    Require(reinterpret_cast<std::uintptr_t>(once) == 2, "a completed initialization marks the object");
    Require(Begin(&once) == 0, "a later caller does not run the initialization");
    return 0;
}
