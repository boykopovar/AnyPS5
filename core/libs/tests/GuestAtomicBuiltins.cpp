#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <thread>

static void Require(bool value) { if (!value) std::abort(); }

template<typename TException, typename TFunction>
static void RequireException(TFunction function) {
    try {
        function();
    } catch (const TException&) {
        return;
    }
    std::abort();
}

static Uint128 Make(std::uint64_t high, std::uint64_t low) { return (static_cast<Uint128>(high) << 64) | low; }

static void CheckLockFree() {
    alignas(16) unsigned char buffer[32] = {};
    Require(__atomic_is_lock_free_nid_postfix(0, buffer + 1) && __atomic_is_lock_free_nid_postfix(1, buffer + 3));
    Require(__atomic_is_lock_free_nid_postfix(1, nullptr) && __atomic_is_lock_free_nid_postfix(16, nullptr));
    Require(__atomic_is_lock_free_nid_postfix(2, buffer + 2) && !__atomic_is_lock_free_nid_postfix(2, buffer + 1));
    Require(__atomic_is_lock_free_nid_postfix(4, buffer + 4) && !__atomic_is_lock_free_nid_postfix(4, buffer + 2));
    Require(__atomic_is_lock_free_nid_postfix(8, buffer + 8) && !__atomic_is_lock_free_nid_postfix(8, buffer + 4));
    Require(__atomic_is_lock_free_nid_postfix(16, buffer + 16) && !__atomic_is_lock_free_nid_postfix(16, buffer + 8));
    Require(!__atomic_is_lock_free_nid_postfix(3, buffer) && !__atomic_is_lock_free_nid_postfix(32, buffer));
}

static void CheckGenericTraps() {
    alignas(16) unsigned char object[24] = {};
    unsigned char other[24] = {};
    RequireException<std::runtime_error>([&] { __atomic_load_nid_postfix(sizeof(object), object, other, 5); });
    RequireException<std::runtime_error>([&] { __atomic_store_nid_postfix(sizeof(object), object, other, 5); });
    RequireException<std::runtime_error>([&] { __atomic_exchange_nid_postfix(sizeof(object), object, other, other, 5); });
    RequireException<std::runtime_error>(
        [&] { __atomic_compare_exchange_nid_postfix(sizeof(object), object, other, other, 5, 5); });
}

static void CheckSixteenByteInvalidTargets() {
    alignas(16) unsigned char sentinel[32];
    for (unsigned char& byte : sentinel) {
        byte = 0xA5;
    }
    auto* unaligned = reinterpret_cast<Uint128*>(sentinel + 8);
    const auto checkBoth = [unaligned](auto fn) {
        RequireException<std::invalid_argument>([&] { fn(nullptr, Make(1, 2)); });
        RequireException<std::invalid_argument>([&] { fn(unaligned, Make(1, 2)); });
    };
    checkBoth(__sync_fetch_and_add_16_nid_postfix);
    checkBoth(__sync_fetch_and_sub_16_nid_postfix);
    checkBoth(__sync_fetch_and_and_16_nid_postfix);
    checkBoth(__sync_fetch_and_or_16_nid_postfix);
    checkBoth(__sync_fetch_and_xor_16_nid_postfix);
    checkBoth(__sync_lock_test_and_set_16_nid_postfix);
    for (unsigned char byte : sentinel) {
        Require(byte == 0xA5);
    }
}

static void CheckSixteenByteZeroSeed() {
    alignas(16) Uint128 zeroAdd = 0;
    Require(__sync_fetch_and_add_16_nid_postfix(&zeroAdd, 0) == 0 && zeroAdd == 0);
    Require(__sync_fetch_and_add_16_nid_postfix(&zeroAdd, Make(1, 2)) == 0 && zeroAdd == Make(1, 2));
    alignas(16) Uint128 zeroSub = 0;
    Require(__sync_fetch_and_sub_16_nid_postfix(&zeroSub, 1) == 0 && zeroSub == Make(~0ull, ~0ull));
    alignas(16) Uint128 zeroAnd = 0;
    Require(__sync_fetch_and_and_16_nid_postfix(&zeroAnd, Make(7, 8)) == 0 && zeroAnd == 0);
    alignas(16) Uint128 zeroOr = 0;
    Require(__sync_fetch_and_or_16_nid_postfix(&zeroOr, Make(2, 3)) == 0 && zeroOr == Make(2, 3));
    alignas(16) Uint128 zeroXor = 0;
    Require(__sync_fetch_and_xor_16_nid_postfix(&zeroXor, Make(4, 5)) == 0 && zeroXor == Make(4, 5));
    alignas(16) Uint128 zeroTas = 0;
    Require(__sync_lock_test_and_set_16_nid_postfix(&zeroTas, Make(6, 7)) == 0 && zeroTas == Make(6, 7));
}

static void CheckSixteenByte() {
    alignas(16) Uint128 value = Make(1, ~0ull);
    Require(__sync_fetch_and_add_16_nid_postfix(&value, 0) == Make(1, ~0ull) && value == Make(1, ~0ull));
    Require(__sync_fetch_and_add_16_nid_postfix(&value, 1) == Make(1, ~0ull) && value == Make(2, 0));
    Require(__sync_fetch_and_sub_16_nid_postfix(&value, 1) == Make(2, 0) && value == Make(1, ~0ull));
    Require(__sync_fetch_and_and_16_nid_postfix(&value, Make(1, 0xff00)) == Make(1, ~0ull) && value == Make(1, 0xff00));
    Require(__sync_fetch_and_or_16_nid_postfix(&value, Make(4, 0xff)) == Make(1, 0xff00) && value == Make(5, 0xffff));
    Require(__sync_fetch_and_xor_16_nid_postfix(&value, Make(5, 0xf0f0)) == Make(5, 0xffff) && value == Make(0, 0x0f0f));
    Require(__sync_fetch_and_xor_16_nid_postfix(&value, 0) == Make(0, 0x0f0f) && value == Make(0, 0x0f0f));
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
    CheckSixteenByteInvalidTargets();
    CheckSixteenByteZeroSeed();
    CheckSixteenByte();
    CheckConcurrentAdds();
    return 0;
}
