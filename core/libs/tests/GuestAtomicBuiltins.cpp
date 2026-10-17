#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <thread>

using Uint128 = unsigned __int128;

extern "C" {
void APS5_VABI __atomic_load_nid_postfix(std::size_t, void*, void*, int);
void APS5_VABI __atomic_store_nid_postfix(std::size_t, void*, void*, int);
void APS5_VABI __atomic_exchange_nid_postfix(std::size_t, void*, void*, void*, int);
bool APS5_VABI __atomic_compare_exchange_nid_postfix(std::size_t, void*, void*, void*, int, int);
bool APS5_VABI __atomic_is_lock_free_nid_postfix(std::size_t, const void*);
Uint128 APS5_VABI __sync_fetch_and_add_16_nid_postfix(Uint128*, Uint128);
Uint128 APS5_VABI __sync_fetch_and_sub_16_nid_postfix(Uint128*, Uint128);
Uint128 APS5_VABI __sync_fetch_and_and_16_nid_postfix(Uint128*, Uint128);
Uint128 APS5_VABI __sync_fetch_and_or_16_nid_postfix(Uint128*, Uint128);
Uint128 APS5_VABI __sync_fetch_and_xor_16_nid_postfix(Uint128*, Uint128);
Uint128 APS5_VABI __sync_lock_test_and_set_16_nid_postfix(Uint128*, Uint128);
}

static void Require(bool value) { if (!value) std::abort(); }

template<typename TFunction>
static void RequireThrows(TFunction function) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return;
    }
    std::abort();
}

static Uint128 Make(std::uint64_t high, std::uint64_t low) { return (static_cast<Uint128>(high) << 64) | low; }

static void CheckLockFree() {
    alignas(16) unsigned char buffer[32] = {};
    Require(__atomic_is_lock_free_nid_postfix(0, buffer + 1) && __atomic_is_lock_free_nid_postfix(1, buffer + 3));
    Require(__atomic_is_lock_free_nid_postfix(2, buffer + 2) && !__atomic_is_lock_free_nid_postfix(2, buffer + 1));
    Require(__atomic_is_lock_free_nid_postfix(4, buffer + 4) && !__atomic_is_lock_free_nid_postfix(4, buffer + 2));
    Require(__atomic_is_lock_free_nid_postfix(8, buffer + 8) && !__atomic_is_lock_free_nid_postfix(8, buffer + 4));
    Require(__atomic_is_lock_free_nid_postfix(16, buffer + 16) && !__atomic_is_lock_free_nid_postfix(16, buffer + 8));
    Require(!__atomic_is_lock_free_nid_postfix(3, buffer) && !__atomic_is_lock_free_nid_postfix(32, buffer));
}

static void CheckGenericTraps() {
    alignas(16) unsigned char object[24] = {};
    unsigned char other[24] = {};
    RequireThrows([&] { __atomic_load_nid_postfix(sizeof(object), object, other, 5); });
    RequireThrows([&] { __atomic_store_nid_postfix(sizeof(object), object, other, 5); });
    RequireThrows([&] { __atomic_exchange_nid_postfix(sizeof(object), object, other, other, 5); });
    RequireThrows([&] { __atomic_compare_exchange_nid_postfix(sizeof(object), object, other, other, 5, 5); });
}

static void CheckSixteenByte() {
    alignas(16) Uint128 value = Make(1, ~0ull);
    Require(__sync_fetch_and_add_16_nid_postfix(&value, 1) == Make(1, ~0ull) && value == Make(2, 0));
    Require(__sync_fetch_and_sub_16_nid_postfix(&value, 1) == Make(2, 0) && value == Make(1, ~0ull));
    Require(__sync_fetch_and_and_16_nid_postfix(&value, Make(1, 0xff00)) == Make(1, ~0ull) && value == Make(1, 0xff00));
    Require(__sync_fetch_and_or_16_nid_postfix(&value, Make(4, 0xff)) == Make(1, 0xff00) && value == Make(5, 0xffff));
    Require(__sync_fetch_and_xor_16_nid_postfix(&value, Make(5, 0xf0f0)) == Make(5, 0xffff) && value == Make(0, 0x0f0f));
    Require(__sync_lock_test_and_set_16_nid_postfix(&value, Make(9, 9)) == Make(0, 0x0f0f) && value == Make(9, 9));
}

static void CheckConcurrentAdds() {
    alignas(16) Uint128 counter = Make(0, ~0ull - 1000);
    constexpr int Iterations = 20000;
    auto worker = [&counter] {
        for (int i = 0; i < Iterations; ++i) __sync_fetch_and_add_16_nid_postfix(&counter, 1);
    };
    std::thread first(worker);
    std::thread second(worker);
    first.join();
    second.join();
    Require(counter == Make(0, ~0ull - 1000) + 2 * Iterations);
}

int main() {
    CheckLockFree();
    CheckGenericTraps();
    CheckSixteenByte();
    CheckConcurrentAdds();
    return 0;
}
