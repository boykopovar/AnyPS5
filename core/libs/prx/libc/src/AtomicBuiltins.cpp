#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

#include "prx/libc/include/General.hpp"

namespace {

[[noreturn]] void ConsoleTrap(const char* function) {
    throw std::runtime_error(std::string(function) + ": the console implementation traps (ud2)");
}

bool CompareExchange16(Uint128* target, Uint128& expected, Uint128 desired) {
    std::uint64_t low = static_cast<std::uint64_t>(expected);
    std::uint64_t high = static_cast<std::uint64_t>(expected >> 64);
    bool exchanged;
    asm volatile("lock cmpxchg16b %[target]"
                 : [target] "+m"(*target), "+a"(low), "+d"(high), "=@ccz"(exchanged)
                 : "b"(static_cast<std::uint64_t>(desired)), "c"(static_cast<std::uint64_t>(desired >> 64))
                 : "memory");
    expected = (static_cast<Uint128>(high) << 64) | low;
    return exchanged;
}

template<typename TOperation>
Uint128 FetchAndApply16(Uint128* target, TOperation operation) {
    if (target == nullptr || (reinterpret_cast<std::uintptr_t>(target) & 15) != 0) {
        throw std::invalid_argument("FetchAndApply16: target must be non-null and 16-byte aligned");
    }
    Uint128 expected = 0;
    while (!CompareExchange16(target, expected, operation(expected))) {
    }
    return expected;
}

}

extern "C" {

void APS5_VABI __atomic_load_nid_postfix(std::size_t, void*, void*, int) { ConsoleTrap("__atomic_load"); }
void APS5_VABI __atomic_store_nid_postfix(std::size_t, void*, void*, int) { ConsoleTrap("__atomic_store"); }
void APS5_VABI __atomic_exchange_nid_postfix(std::size_t, void*, void*, void*, int) { ConsoleTrap("__atomic_exchange"); }

bool APS5_VABI __atomic_compare_exchange_nid_postfix(std::size_t, void*, void*, void*, int, int) {
    ConsoleTrap("__atomic_compare_exchange");
}

bool APS5_VABI __atomic_is_lock_free_nid_postfix(std::size_t size, const void* pointer) {
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(pointer);
    switch (size) {
    case 0:
    case 1:
        return true;
    case 2:
    case 4:
    case 8:
    case 16:
        return (address & (size - 1)) == 0;
    default:
        return false;
    }
}

Uint128 APS5_VABI __sync_fetch_and_add_16_nid_postfix(Uint128* target, Uint128 value) {
    return FetchAndApply16(target, [value](Uint128 current) { return current + value; });
}

Uint128 APS5_VABI __sync_fetch_and_sub_16_nid_postfix(Uint128* target, Uint128 value) {
    return FetchAndApply16(target, [value](Uint128 current) { return current - value; });
}

Uint128 APS5_VABI __sync_fetch_and_and_16_nid_postfix(Uint128* target, Uint128 value) {
    return FetchAndApply16(target, [value](Uint128 current) { return current & value; });
}

Uint128 APS5_VABI __sync_fetch_and_or_16_nid_postfix(Uint128* target, Uint128 value) {
    return FetchAndApply16(target, [value](Uint128 current) { return current | value; });
}

Uint128 APS5_VABI __sync_fetch_and_xor_16_nid_postfix(Uint128* target, Uint128 value) {
    return FetchAndApply16(target, [value](Uint128 current) { return current ^ value; });
}

Uint128 APS5_VABI __sync_lock_test_and_set_16_nid_postfix(Uint128* target, Uint128 value) {
    return FetchAndApply16(target, [value](Uint128) { return value; });
}

}
