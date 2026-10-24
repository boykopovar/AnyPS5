#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <thread>

extern "C" {
void APS5_VABI _Atomic_store_1_nid_postfix(volatile std::uint8_t*, std::uint8_t, int);
void APS5_VABI _Atomic_store_8_nid_postfix(volatile std::uint64_t*, std::uint64_t, int);
std::uint16_t APS5_VABI _Atomic_load_2_nid_postfix(volatile std::uint16_t*, int);
std::uint64_t APS5_VABI _Atomic_load_8_nid_postfix(volatile std::uint64_t*, int);
std::uint32_t APS5_VABI _Atomic_exchange_4_nid_postfix(volatile std::uint32_t*, std::uint32_t, int);
std::uint64_t APS5_VABI _Atomic_exchange_8_nid_postfix(volatile std::uint64_t*, std::uint64_t, int);
int APS5_VABI _Atomic_compare_exchange_strong_2_nid_postfix(volatile std::uint16_t*, std::uint16_t*, std::uint16_t, int, int);
int APS5_VABI _Atomic_compare_exchange_weak_8_nid_postfix(volatile std::uint64_t*, std::uint64_t*, std::uint64_t, int, int);
std::uint8_t APS5_VABI _Atomic_fetch_add_1_nid_postfix(volatile std::uint8_t*, std::uint8_t, int);
std::uint64_t APS5_VABI _Atomic_fetch_add_8_nid_postfix(volatile std::uint64_t*, std::uint64_t, int);
std::uint16_t APS5_VABI _Atomic_fetch_sub_2_nid_postfix(volatile std::uint16_t*, std::uint16_t, int);
std::uint32_t APS5_VABI _Atomic_fetch_and_4_nid_postfix(volatile std::uint32_t*, std::uint32_t, int);
std::uint8_t APS5_VABI _Atomic_fetch_or_1_nid_postfix(volatile std::uint8_t*, std::uint8_t, int);
std::uint64_t APS5_VABI _Atomic_fetch_xor_8_nid_postfix(volatile std::uint64_t*, std::uint64_t, int);
int APS5_VABI _Atomic_is_lock_free_4_nid_postfix();
int APS5_VABI _Atomic_flag_test_and_set_nid_postfix(volatile unsigned char*, int);
void APS5_VABI _Atomic_flag_clear_nid_postfix(volatile unsigned char*, int);
void APS5_VABI _Atomic_thread_fence_nid_postfix(int);
void APS5_VABI _Atomic_signal_fence_nid_postfix(int);
void APS5_VABI _Atomic_copy_nid_postfix(volatile unsigned char*, std::size_t, volatile void*, const volatile void*, int);
void APS5_VABI _Atomic_exchange_nid_postfix(volatile unsigned char*, std::size_t, volatile void*, volatile void*, int);
int APS5_VABI _Atomic_compare_exchange_strong_nid_postfix(volatile unsigned char*, std::size_t, volatile void*, void*, const void*, int, int);
int APS5_VABI _Atomic_compare_exchange_weak_nid_postfix(volatile unsigned char*, std::size_t, volatile void*, void*, const void*, int, int);
}

static void Require(bool value) { if (!value) std::abort(); }

constexpr int Relaxed = 0, Acquire = 2, Release = 3, AcqRel = 4, SeqCst = 5;

static void CheckTyped() {
    std::uint8_t byte = 0;
    _Atomic_store_1_nid_postfix(&byte, 7, SeqCst);
    Require(byte == 7 && _Atomic_fetch_add_1_nid_postfix(&byte, 250, Relaxed) == 7 && byte == 1);
    Require(_Atomic_fetch_or_1_nid_postfix(&byte, 0x80, AcqRel) == 1 && byte == 0x81);
    std::uint16_t half = 5;
    Require(_Atomic_load_2_nid_postfix(&half, Release) == 5 && _Atomic_fetch_sub_2_nid_postfix(&half, 6, SeqCst) == 5 && half == 0xffff);
    std::uint16_t expectedHalf = 1;
    Require(_Atomic_compare_exchange_strong_2_nid_postfix(&half, &expectedHalf, 2, SeqCst, Relaxed) == 0 && expectedHalf == 0xffff);
    Require(_Atomic_compare_exchange_strong_2_nid_postfix(&half, &expectedHalf, 2, SeqCst, Relaxed) == 1 && half == 2);
    std::uint32_t word = 0xff0f;
    Require(_Atomic_fetch_and_4_nid_postfix(&word, 0xf0ff, Acquire) == 0xff0f && word == 0xf00f);
    Require(_Atomic_exchange_4_nid_postfix(&word, 9, 6) == 0xf00f && word == 9);
    std::uint64_t wide = 0;
    _Atomic_store_8_nid_postfix(&wide, 0x123456789ull, Release);
    Require(_Atomic_load_8_nid_postfix(&wide, Acquire) == 0x123456789ull);
    Require(_Atomic_load_8_nid_postfix(&wide, Release) == 0 && _Atomic_load_8_nid_postfix(&wide, AcqRel) == 0);
    Require(_Atomic_fetch_add_8_nid_postfix(&wide, 1, SeqCst) == 0x123456789ull && wide == 0x12345678aull);
    Require(_Atomic_fetch_add_8_nid_postfix(&wide, 1, 6) == 0 && wide == 0x12345678aull);
    Require(_Atomic_exchange_8_nid_postfix(&wide, 1, 7) == 0 && wide == 0x12345678aull);
    Require(_Atomic_fetch_xor_8_nid_postfix(&wide, 0xf, Relaxed) == 0x12345678aull && wide == 0x123456785ull);
    std::uint64_t expectedWide = 0x123456785ull;
    Require(_Atomic_compare_exchange_weak_8_nid_postfix(&wide, &expectedWide, 3, SeqCst, SeqCst) == 1 && wide == 3);
    Require(_Atomic_is_lock_free_4_nid_postfix() == 1);
}

static void CheckFlagsAndFences() {
    unsigned char flag = 0;
    Require(_Atomic_flag_test_and_set_nid_postfix(&flag, SeqCst) == 0 && flag == 1);
    Require(_Atomic_flag_test_and_set_nid_postfix(&flag, Relaxed) == 1);
    _Atomic_flag_clear_nid_postfix(&flag, Release);
    Require(flag == 0);
    _Atomic_thread_fence_nid_postfix(SeqCst);
    for (int order = Relaxed; order <= SeqCst; ++order) _Atomic_signal_fence_nid_postfix(order);
}

struct Wide {
    std::uint64_t parts[3];
};

static void CheckGeneric() {
    unsigned char lock = 0;
    Wide target{{1, 2, 3}}, source{{4, 5, 6}}, expected{{1, 2, 3}};
    _Atomic_copy_nid_postfix(&lock, sizeof(Wide), &target, &source, SeqCst);
    Require(std::memcmp(&target, &source, sizeof(Wide)) == 0 && lock == 0);
    Wide other{{7, 8, 9}};
    _Atomic_exchange_nid_postfix(&lock, sizeof(Wide), &target, &other, SeqCst);
    Require(target.parts[0] == 7 && other.parts[0] == 4 && lock == 0);
    const Wide desired{{10, 11, 12}};
    Require(_Atomic_compare_exchange_strong_nid_postfix(&lock, sizeof(Wide), &target, &expected, &desired, SeqCst, SeqCst) == 0);
    Require(std::memcmp(&expected, &target, sizeof(Wide)) == 0 && lock == 0);
    Require(_Atomic_compare_exchange_weak_nid_postfix(&lock, sizeof(Wide), &target, &expected, &desired, SeqCst, SeqCst) == 1);
    Require(std::memcmp(&target, &desired, sizeof(Wide)) == 0 && lock == 0);
}

static void CheckConcurrentGeneric() {
    unsigned char lock = 0;
    Wide counter{{0, 0, 0}};
    constexpr int Iterations = 20000;
    auto worker = [&] {
        for (int i = 0; i < Iterations; ++i) {
            Wide expected;
            _Atomic_copy_nid_postfix(&lock, sizeof(Wide), &expected, &counter, SeqCst);
            Wide next;
            do {
                next = Wide{{expected.parts[0] + 1, expected.parts[1] + 2, expected.parts[2] + 3}};
            } while (!_Atomic_compare_exchange_strong_nid_postfix(&lock, sizeof(Wide), &counter, &expected, &next, SeqCst, SeqCst));
        }
    };
    std::thread first(worker);
    std::thread second(worker);
    first.join();
    second.join();
    Require(counter.parts[0] == 2 * Iterations && counter.parts[1] == 4 * Iterations && counter.parts[2] == 6 * Iterations);
}

int main() {
    CheckTyped();
    CheckFlagsAndFences();
    CheckGeneric();
    CheckConcurrentGeneric();
    return 0;
}
