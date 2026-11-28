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
    alignas(4) std::uint8_t bytes[4] = {0x2a, 0xff, 0xff, 0xff};
    Require(__std_atomic_wait_direct_8_nid_postfix(&bytes[0], 0x2b, 0xffffffff) == 1, "a different 8-bit value returns at once");

    const auto byteBefore = std::chrono::steady_clock::now();
    Require(__std_atomic_wait_direct_8_nid_postfix(&bytes[0], 0x2a, 20) == 1, "a timed out 8-bit wait returns 1");
    Require(std::chrono::steady_clock::now() - byteBefore >= std::chrono::milliseconds(15), "the 8-bit timeout is honored");

    std::atomic<bool> byteWoken{false};
    std::thread byteWaiter([&] {
        __std_atomic_wait_direct_8_nid_postfix(&bytes[0], 0x2a, 0xffffffff);
        byteWoken.store(true);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(!byteWoken.load(), "the 8-bit waiter blocks while the low byte matches despite non-zero adjacent bytes");
    std::atomic_ref<std::uint8_t>(bytes[0]).store(0x2b);
    __std_atomic_notify_one_direct_nid_postfix(&bytes[0]);
    byteWaiter.join();
    Require(byteWoken.load(), "notify one wakes the 8-bit waiter");

    alignas(4) std::uint16_t halves[2] = {0x1234, 0xffff};
    Require(__std_atomic_wait_direct_16_nid_postfix(&halves[0], 0x1235, 0xffffffff) == 1, "a different 16-bit value returns at once");

    const auto halfBefore = std::chrono::steady_clock::now();
    Require(__std_atomic_wait_direct_16_nid_postfix(&halves[0], 0x1234, 20) == 1, "a timed out 16-bit wait returns 1");
    Require(std::chrono::steady_clock::now() - halfBefore >= std::chrono::milliseconds(15), "the 16-bit timeout is honored");

    std::atomic<bool> halfWoken{false};
    std::thread halfWaiter([&] {
        __std_atomic_wait_direct_16_nid_postfix(&halves[0], 0x1234, 0xffffffff);
        halfWoken.store(true);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(!halfWoken.load(), "the 16-bit waiter blocks while the low halfword matches despite non-zero adjacent halfword");
    std::atomic_ref<std::uint16_t>(halves[0]).store(0x1235);
    __std_atomic_notify_one_direct_nid_postfix(&halves[0]);
    halfWaiter.join();
    Require(halfWoken.load(), "notify one wakes the 16-bit waiter");

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

    alignas(8) std::int64_t libcxx = -5;
    Require(_ZNSt3__123__libcpp_atomic_monitorEPVKNS_17__cxx_atomic_implIlNS_22__cxx_atomic_base_implIlEEEE_nid_postfix(&libcxx) == -5,
            "libc++ monitor reads the initial value");
    _ZNSt3__120__libcpp_atomic_waitEPVKNS_17__cxx_atomic_implIlNS_22__cxx_atomic_base_implIlEEEEl_nid_postfix(&libcxx, -4);
    std::atomic<bool> libcxxWoken{false};
    std::thread libcxxWaiter([&] {
        _ZNSt3__120__libcpp_atomic_waitEPVKNS_17__cxx_atomic_implIlNS_22__cxx_atomic_base_implIlEEEEl_nid_postfix(&libcxx, -5);
        libcxxWoken.store(true);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(!libcxxWoken.load(), "libc++ waiter blocks while the value matches");
    std::atomic_ref<std::int64_t>(libcxx).store(6);
    _ZNSt3__123__cxx_atomic_notify_oneEPVKNS_17__cxx_atomic_implIlNS_22__cxx_atomic_base_implIlEEEE_nid_postfix(&libcxx);
    libcxxWaiter.join();
    Require(libcxxWoken.load() && _ZNSt3__123__libcpp_atomic_monitorEPVKNS_17__cxx_atomic_implIlNS_22__cxx_atomic_base_implIlEEEE_nid_postfix(&libcxx) == 6,
            "libc++ notify one wakes the waiter after a changed value");
    return 0;
}
