#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace {

void Require(bool condition, const char* label) {
    if (condition) return;
    std::fprintf(stderr, "std atomic wait with libkernel: %s\n", label);
    std::abort();
}

}

int main() {
    alignas(8) std::uint32_t word = 1;
    Require(__std_atomic_wait_direct_32_nid_postfix(&word, 2, 0xffffffff) == 1, "a different value returns at once");

    const auto before = std::chrono::steady_clock::now();
    Require(__std_atomic_wait_direct_32_nid_postfix(&word, 1, 20) == 1, "a timed out wait returns 1");
    Require(std::chrono::steady_clock::now() - before >= std::chrono::milliseconds(15), "the timeout is honored");

    std::atomic<bool> woken{false};
    std::thread waiter([&] {
        __std_atomic_wait_direct_32_nid_postfix(&word, 1, 0xffffffff);
        woken.store(true);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(!woken.load(), "the waiter blocks while the value matches");
    std::atomic_ref<std::uint32_t>(word).store(3);
    __std_atomic_notify_all_direct_nid_postfix(&word);
    waiter.join();
    Require(woken.load(), "notify all wakes the waiter");

    woken.store(false);
    std::thread kernelWoken([&] {
        __std_atomic_wait_direct_32_nid_postfix(&word, 3, 0xffffffff);
        woken.store(true);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(!woken.load(), "the second waiter blocks");
    std::atomic_ref<std::uint32_t>(word).store(4);
    sceKernelSyncOnAddressWake(&word, 1);
    kernelWoken.join();
    Require(woken.load(), "libkernel's wake reaches a libc waiter");

    alignas(8) std::uint64_t wide = 5;
    std::thread wideWaiter([&] { sceKernelSyncOnAddressWait64(&wide, 5, nullptr, "test"); });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    std::atomic_ref<std::uint64_t>(wide).store(6);
    __std_atomic_notify_one_direct_nid_postfix(&wide);
    wideWaiter.join();
    Require(__std_atomic_wait_direct_64_nid_postfix(&wide, 5, 0xffffffff) == 1, "a different 64-bit value returns at once");

    const auto wideBefore = std::chrono::steady_clock::now();
    Require(__std_atomic_wait_direct_64_nid_postfix(&wide, 6, 20) == 1, "a timed out 64-bit wait returns 1");
    Require(std::chrono::steady_clock::now() - wideBefore >= std::chrono::milliseconds(15), "the 64-bit timeout is honored");

    std::atomic<bool> wideWoken{false};
    std::thread directWideWaiter([&] {
        __std_atomic_wait_direct_64_nid_postfix(&wide, 6, 0xffffffff);
        wideWoken.store(true);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(!wideWoken.load(), "the 64-bit waiter blocks while the value matches");
    std::atomic_ref<std::uint64_t>(wide).store(7);
    __std_atomic_notify_all_direct_nid_postfix(&wide);
    directWideWaiter.join();
    Require(wideWoken.load(), "notify all wakes the 64-bit waiter");
    return 0;
}
