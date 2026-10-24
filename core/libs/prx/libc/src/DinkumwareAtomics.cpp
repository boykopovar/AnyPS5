#include <cstddef>
#include <cstdint>
#include <cstring>

#include "prx/libc/include/General.hpp"

namespace {

constexpr int MemoryOrderSeqCst = 5;

template<typename TValue>
void Store(volatile TValue* target, TValue value) {
    __atomic_store_n(target, value, __ATOMIC_SEQ_CST);
}

template<typename TValue>
TValue Load(volatile TValue* target) {
    return __atomic_load_n(target, __ATOMIC_SEQ_CST);
}

bool IsValidOrder(int order) {
    return order >= 0 && order <= MemoryOrderSeqCst;
}

template<typename TValue>
TValue Exchange(volatile TValue* target, TValue value) {
    return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
}

template<typename TValue>
int CompareExchange(volatile TValue* target, TValue* expected, TValue desired) {
    return __atomic_compare_exchange_n(target, expected, desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST) ? 1 : 0;
}

void LockFlag(volatile unsigned char* flag) {
    while (__atomic_exchange_n(flag, static_cast<unsigned char>(1), __ATOMIC_ACQUIRE) != 0) {
    }
}

void UnlockFlag(volatile unsigned char* flag) {
    __atomic_store_n(flag, static_cast<unsigned char>(0), __ATOMIC_SEQ_CST);
}

void CopyBytes(volatile void* destination, const volatile void* source, std::size_t size) {
    std::memcpy(const_cast<void*>(destination), const_cast<const void*>(source), size);
}

int LockedCompareExchange(volatile unsigned char* flag, std::size_t size, volatile void* target, void* expected, const void* desired) {
    LockFlag(flag);
    void* object = const_cast<void*>(target);
    const bool equal = std::memcmp(object, expected, size) == 0;
    if (equal) {
        std::memcpy(object, desired, size);
    } else {
        std::memcpy(expected, object, size);
    }
    UnlockFlag(flag);
    return equal ? 1 : 0;
}

}

extern "C" {

std::uint8_t APS5_VABI _Fetch_and_seq_cst_1_nid_postfix(volatile std::uint8_t* target, std::uint8_t value) { return __atomic_fetch_and(target, value, __ATOMIC_SEQ_CST); }
std::uint16_t APS5_VABI _Fetch_and_seq_cst_2_nid_postfix(volatile std::uint16_t* target, std::uint16_t value) { return __atomic_fetch_and(target, value, __ATOMIC_SEQ_CST); }
std::uint32_t APS5_VABI _Fetch_and_seq_cst_4_nid_postfix(volatile std::uint32_t* target, std::uint32_t value) { return __atomic_fetch_and(target, value, __ATOMIC_SEQ_CST); }

std::uint8_t APS5_VABI _Fetch_or_seq_cst_1_nid_postfix(volatile std::uint8_t* target, std::uint8_t value) { return __atomic_fetch_or(target, value, __ATOMIC_SEQ_CST); }
std::uint16_t APS5_VABI _Fetch_or_seq_cst_2_nid_postfix(volatile std::uint16_t* target, std::uint16_t value) { return __atomic_fetch_or(target, value, __ATOMIC_SEQ_CST); }
std::uint32_t APS5_VABI _Fetch_or_seq_cst_4_nid_postfix(volatile std::uint32_t* target, std::uint32_t value) { return __atomic_fetch_or(target, value, __ATOMIC_SEQ_CST); }

std::uint8_t APS5_VABI _Fetch_xor_seq_cst_1_nid_postfix(volatile std::uint8_t* target, std::uint8_t value) { return __atomic_fetch_xor(target, value, __ATOMIC_SEQ_CST); }
std::uint16_t APS5_VABI _Fetch_xor_seq_cst_2_nid_postfix(volatile std::uint16_t* target, std::uint16_t value) { return __atomic_fetch_xor(target, value, __ATOMIC_SEQ_CST); }
std::uint32_t APS5_VABI _Fetch_xor_seq_cst_4_nid_postfix(volatile std::uint32_t* target, std::uint32_t value) { return __atomic_fetch_xor(target, value, __ATOMIC_SEQ_CST); }

void APS5_VABI _Atomic_store_1_nid_postfix(volatile std::uint8_t* target, std::uint8_t value, int) { Store(target, value); }
void APS5_VABI _Atomic_store_2_nid_postfix(volatile std::uint16_t* target, std::uint16_t value, int) { Store(target, value); }
void APS5_VABI _Atomic_store_4_nid_postfix(volatile std::uint32_t* target, std::uint32_t value, int) { Store(target, value); }
void APS5_VABI _Atomic_store_8_nid_postfix(volatile std::uint64_t* target, std::uint64_t value, int) { Store(target, value); }

std::uint8_t APS5_VABI _Atomic_load_1_nid_postfix(volatile std::uint8_t* target, int) { return Load(target); }
std::uint16_t APS5_VABI _Atomic_load_2_nid_postfix(volatile std::uint16_t* target, int) { return Load(target); }
std::uint64_t APS5_VABI _Atomic_load_8_nid_postfix(volatile std::uint64_t* target, int order) {
    if (order == 3 || order == 4 || !IsValidOrder(order)) return 0;
    return Load(target);
}

std::uint8_t APS5_VABI _Atomic_exchange_1_nid_postfix(volatile std::uint8_t* target, std::uint8_t value, int) { return Exchange(target, value); }
std::uint16_t APS5_VABI _Atomic_exchange_2_nid_postfix(volatile std::uint16_t* target, std::uint16_t value, int) { return Exchange(target, value); }
std::uint32_t APS5_VABI _Atomic_exchange_4_nid_postfix(volatile std::uint32_t* target, std::uint32_t value, int) { return Exchange(target, value); }
std::uint64_t APS5_VABI _Atomic_exchange_8_nid_postfix(volatile std::uint64_t* target, std::uint64_t value, int order) { return IsValidOrder(order) ? Exchange(target, value) : 0; }

int APS5_VABI _Atomic_compare_exchange_strong_1_nid_postfix(volatile std::uint8_t* target, std::uint8_t* expected, std::uint8_t desired, int, int) {
    return CompareExchange(target, expected, desired);
}

int APS5_VABI _Atomic_compare_exchange_strong_2_nid_postfix(volatile std::uint16_t* target, std::uint16_t* expected, std::uint16_t desired, int, int) {
    return CompareExchange(target, expected, desired);
}

int APS5_VABI _Atomic_compare_exchange_strong_4_nid_postfix(volatile std::uint32_t* target, std::uint32_t* expected, std::uint32_t desired, int, int) {
    return CompareExchange(target, expected, desired);
}

int APS5_VABI _Atomic_compare_exchange_strong_8_nid_postfix(volatile std::uint64_t* target, std::uint64_t* expected, std::uint64_t desired, int, int) {
    return CompareExchange(target, expected, desired);
}

int APS5_VABI _Atomic_compare_exchange_weak_1_nid_postfix(volatile std::uint8_t* target, std::uint8_t* expected, std::uint8_t desired, int, int) {
    return CompareExchange(target, expected, desired);
}

int APS5_VABI _Atomic_compare_exchange_weak_2_nid_postfix(volatile std::uint16_t* target, std::uint16_t* expected, std::uint16_t desired, int, int) {
    return CompareExchange(target, expected, desired);
}

int APS5_VABI _Atomic_compare_exchange_weak_8_nid_postfix(volatile std::uint64_t* target, std::uint64_t* expected, std::uint64_t desired, int, int) {
    return CompareExchange(target, expected, desired);
}

std::uint8_t APS5_VABI _Atomic_fetch_add_1_nid_postfix(volatile std::uint8_t* target, std::uint8_t value, int) { return __atomic_fetch_add(target, value, __ATOMIC_SEQ_CST); }
std::uint16_t APS5_VABI _Atomic_fetch_add_2_nid_postfix(volatile std::uint16_t* target, std::uint16_t value, int) { return __atomic_fetch_add(target, value, __ATOMIC_SEQ_CST); }
std::uint64_t APS5_VABI _Atomic_fetch_add_8_nid_postfix(volatile std::uint64_t* target, std::uint64_t value, int order) { return IsValidOrder(order) ? __atomic_fetch_add(target, value, __ATOMIC_SEQ_CST) : 0; }
std::uint8_t APS5_VABI _Atomic_fetch_sub_1_nid_postfix(volatile std::uint8_t* target, std::uint8_t value, int) { return __atomic_fetch_sub(target, value, __ATOMIC_SEQ_CST); }
std::uint16_t APS5_VABI _Atomic_fetch_sub_2_nid_postfix(volatile std::uint16_t* target, std::uint16_t value, int) { return __atomic_fetch_sub(target, value, __ATOMIC_SEQ_CST); }
std::uint64_t APS5_VABI _Atomic_fetch_sub_8_nid_postfix(volatile std::uint64_t* target, std::uint64_t value, int order) { return IsValidOrder(order) ? __atomic_fetch_sub(target, value, __ATOMIC_SEQ_CST) : 0; }
std::uint8_t APS5_VABI _Atomic_fetch_and_1_nid_postfix(volatile std::uint8_t* target, std::uint8_t value, int) { return __atomic_fetch_and(target, value, __ATOMIC_SEQ_CST); }
std::uint16_t APS5_VABI _Atomic_fetch_and_2_nid_postfix(volatile std::uint16_t* target, std::uint16_t value, int) { return __atomic_fetch_and(target, value, __ATOMIC_SEQ_CST); }
std::uint32_t APS5_VABI _Atomic_fetch_and_4_nid_postfix(volatile std::uint32_t* target, std::uint32_t value, int) { return __atomic_fetch_and(target, value, __ATOMIC_SEQ_CST); }
std::uint64_t APS5_VABI _Atomic_fetch_and_8_nid_postfix(volatile std::uint64_t* target, std::uint64_t value, int order) { return IsValidOrder(order) ? __atomic_fetch_and(target, value, __ATOMIC_SEQ_CST) : 0; }
std::uint8_t APS5_VABI _Atomic_fetch_or_1_nid_postfix(volatile std::uint8_t* target, std::uint8_t value, int) { return __atomic_fetch_or(target, value, __ATOMIC_SEQ_CST); }
std::uint16_t APS5_VABI _Atomic_fetch_or_2_nid_postfix(volatile std::uint16_t* target, std::uint16_t value, int) { return __atomic_fetch_or(target, value, __ATOMIC_SEQ_CST); }
std::uint32_t APS5_VABI _Atomic_fetch_or_4_nid_postfix(volatile std::uint32_t* target, std::uint32_t value, int) { return __atomic_fetch_or(target, value, __ATOMIC_SEQ_CST); }
std::uint64_t APS5_VABI _Atomic_fetch_or_8_nid_postfix(volatile std::uint64_t* target, std::uint64_t value, int order) { return IsValidOrder(order) ? __atomic_fetch_or(target, value, __ATOMIC_SEQ_CST) : 0; }
std::uint8_t APS5_VABI _Atomic_fetch_xor_1_nid_postfix(volatile std::uint8_t* target, std::uint8_t value, int) { return __atomic_fetch_xor(target, value, __ATOMIC_SEQ_CST); }
std::uint16_t APS5_VABI _Atomic_fetch_xor_2_nid_postfix(volatile std::uint16_t* target, std::uint16_t value, int) { return __atomic_fetch_xor(target, value, __ATOMIC_SEQ_CST); }
std::uint32_t APS5_VABI _Atomic_fetch_xor_4_nid_postfix(volatile std::uint32_t* target, std::uint32_t value, int) { return __atomic_fetch_xor(target, value, __ATOMIC_SEQ_CST); }
std::uint64_t APS5_VABI _Atomic_fetch_xor_8_nid_postfix(volatile std::uint64_t* target, std::uint64_t value, int order) { return IsValidOrder(order) ? __atomic_fetch_xor(target, value, __ATOMIC_SEQ_CST) : 0; }

int APS5_VABI _Atomic_is_lock_free_1_nid_postfix() { return 1; }
int APS5_VABI _Atomic_is_lock_free_2_nid_postfix() { return 1; }
int APS5_VABI _Atomic_is_lock_free_4_nid_postfix() { return 1; }
int APS5_VABI _Atomic_is_lock_free_8_nid_postfix() { return 1; }

int APS5_VABI _Atomic_flag_test_and_set_nid_postfix(volatile unsigned char* flag, int) {
    return __atomic_exchange_n(flag, static_cast<unsigned char>(1), __ATOMIC_SEQ_CST);
}

void APS5_VABI _Atomic_flag_clear_nid_postfix(volatile unsigned char* flag, int) {
    __atomic_store_n(flag, static_cast<unsigned char>(0), __ATOMIC_SEQ_CST);
}

void APS5_VABI _Atomic_thread_fence_nid_postfix(int order) {
    if (order == MemoryOrderSeqCst) __atomic_thread_fence(__ATOMIC_SEQ_CST);
}

void APS5_VABI _Atomic_signal_fence_nid_postfix(int) {
}

void APS5_VABI _Atomic_copy_nid_postfix(volatile unsigned char* flag, std::size_t size, volatile void* target, const volatile void* source, int) {
    LockFlag(flag);
    CopyBytes(target, source, size);
    UnlockFlag(flag);
}

void APS5_VABI _Atomic_exchange_nid_postfix(volatile unsigned char* flag, std::size_t size, volatile void* target, volatile void* value, int) {
    LockFlag(flag);
    auto* object = static_cast<unsigned char*>(const_cast<void*>(target));
    auto* other = static_cast<unsigned char*>(const_cast<void*>(value));
    for (std::size_t i = 0; i < size; ++i) {
        const unsigned char byte = object[i];
        object[i] = other[i];
        other[i] = byte;
    }
    UnlockFlag(flag);
}

int APS5_VABI _Atomic_compare_exchange_strong_nid_postfix(volatile unsigned char* flag, std::size_t size, volatile void* target, void* expected, const void* desired, int, int) {
    return LockedCompareExchange(flag, size, target, expected, desired);
}

int APS5_VABI _Atomic_compare_exchange_weak_nid_postfix(volatile unsigned char* flag, std::size_t size, volatile void* target, void* expected, const void* desired, int, int) {
    return LockedCompareExchange(flag, size, target, expected, desired);
}


std::uint8_t APS5_VABI __atomic_load_1_nid_postfix(const volatile std::uint8_t* target, int) { return __atomic_load_n(target, __ATOMIC_SEQ_CST); }
std::uint16_t APS5_VABI __atomic_load_2_nid_postfix(const volatile std::uint16_t* target, int) { return __atomic_load_n(target, __ATOMIC_SEQ_CST); }
std::uint32_t APS5_VABI __atomic_load_4_nid_postfix(const volatile std::uint32_t* target, int) { return __atomic_load_n(target, __ATOMIC_SEQ_CST); }
std::uint64_t APS5_VABI __atomic_load_8_nid_postfix(const volatile std::uint64_t* target, int) { return __atomic_load_n(target, __ATOMIC_SEQ_CST); }

void APS5_VABI __atomic_store_1_nid_postfix(volatile std::uint8_t* target, std::uint8_t value, int) { Store(target, value); }
void APS5_VABI __atomic_store_2_nid_postfix(volatile std::uint16_t* target, std::uint16_t value, int) { Store(target, value); }
void APS5_VABI __atomic_store_4_nid_postfix(volatile std::uint32_t* target, std::uint32_t value, int) { Store(target, value); }
void APS5_VABI __atomic_store_8_nid_postfix(volatile std::uint64_t* target, std::uint64_t value, int) { Store(target, value); }

std::uint8_t APS5_VABI __atomic_exchange_1_nid_postfix(volatile std::uint8_t* target, std::uint8_t value, int) { return Exchange(target, value); }
std::uint16_t APS5_VABI __atomic_exchange_2_nid_postfix(volatile std::uint16_t* target, std::uint16_t value, int) { return Exchange(target, value); }
std::uint32_t APS5_VABI __atomic_exchange_4_nid_postfix(volatile std::uint32_t* target, std::uint32_t value, int) { return Exchange(target, value); }
std::uint64_t APS5_VABI __atomic_exchange_8_nid_postfix(volatile std::uint64_t* target, std::uint64_t value, int) { return Exchange(target, value); }

int APS5_VABI __atomic_compare_exchange_1_nid_postfix(volatile std::uint8_t* target, std::uint8_t* expected, std::uint8_t desired, int, int) {
    return CompareExchange(target, expected, desired);
}

int APS5_VABI __atomic_compare_exchange_2_nid_postfix(volatile std::uint16_t* target, std::uint16_t* expected, std::uint16_t desired, int, int) {
    return CompareExchange(target, expected, desired);
}

int APS5_VABI __atomic_compare_exchange_4_nid_postfix(volatile std::uint32_t* target, std::uint32_t* expected, std::uint32_t desired, int, int) {
    return CompareExchange(target, expected, desired);
}

int APS5_VABI __atomic_compare_exchange_8_nid_postfix(volatile std::uint64_t* target, std::uint64_t* expected, std::uint64_t desired, int, int) {
    return CompareExchange(target, expected, desired);
}

std::uint8_t APS5_VABI __atomic_fetch_add_1_nid_postfix(volatile std::uint8_t* target, std::uint8_t value, int) { return __atomic_fetch_add(target, value, __ATOMIC_SEQ_CST); }
std::uint16_t APS5_VABI __atomic_fetch_add_2_nid_postfix(volatile std::uint16_t* target, std::uint16_t value, int) { return __atomic_fetch_add(target, value, __ATOMIC_SEQ_CST); }
std::uint32_t APS5_VABI __atomic_fetch_add_4_nid_postfix(volatile std::uint32_t* target, std::uint32_t value, int) { return __atomic_fetch_add(target, value, __ATOMIC_SEQ_CST); }
std::uint64_t APS5_VABI __atomic_fetch_add_8_nid_postfix(volatile std::uint64_t* target, std::uint64_t value, int) { return __atomic_fetch_add(target, value, __ATOMIC_SEQ_CST); }

std::uint8_t APS5_VABI __atomic_fetch_sub_1_nid_postfix(volatile std::uint8_t* target, std::uint8_t value, int) { return __atomic_fetch_sub(target, value, __ATOMIC_SEQ_CST); }
std::uint16_t APS5_VABI __atomic_fetch_sub_2_nid_postfix(volatile std::uint16_t* target, std::uint16_t value, int) { return __atomic_fetch_sub(target, value, __ATOMIC_SEQ_CST); }
std::uint32_t APS5_VABI __atomic_fetch_sub_4_nid_postfix(volatile std::uint32_t* target, std::uint32_t value, int) { return __atomic_fetch_sub(target, value, __ATOMIC_SEQ_CST); }
std::uint64_t APS5_VABI __atomic_fetch_sub_8_nid_postfix(volatile std::uint64_t* target, std::uint64_t value, int) { return __atomic_fetch_sub(target, value, __ATOMIC_SEQ_CST); }

std::uint8_t APS5_VABI __atomic_fetch_and_1_nid_postfix(volatile std::uint8_t* target, std::uint8_t value, int) { return __atomic_fetch_and(target, value, __ATOMIC_SEQ_CST); }
std::uint16_t APS5_VABI __atomic_fetch_and_2_nid_postfix(volatile std::uint16_t* target, std::uint16_t value, int) { return __atomic_fetch_and(target, value, __ATOMIC_SEQ_CST); }
std::uint32_t APS5_VABI __atomic_fetch_and_4_nid_postfix(volatile std::uint32_t* target, std::uint32_t value, int) { return __atomic_fetch_and(target, value, __ATOMIC_SEQ_CST); }
std::uint64_t APS5_VABI __atomic_fetch_and_8_nid_postfix(volatile std::uint64_t* target, std::uint64_t value, int) { return __atomic_fetch_and(target, value, __ATOMIC_SEQ_CST); }

std::uint8_t APS5_VABI __atomic_fetch_or_1_nid_postfix(volatile std::uint8_t* target, std::uint8_t value, int) { return __atomic_fetch_or(target, value, __ATOMIC_SEQ_CST); }
std::uint16_t APS5_VABI __atomic_fetch_or_2_nid_postfix(volatile std::uint16_t* target, std::uint16_t value, int) { return __atomic_fetch_or(target, value, __ATOMIC_SEQ_CST); }
std::uint32_t APS5_VABI __atomic_fetch_or_4_nid_postfix(volatile std::uint32_t* target, std::uint32_t value, int) { return __atomic_fetch_or(target, value, __ATOMIC_SEQ_CST); }
std::uint64_t APS5_VABI __atomic_fetch_or_8_nid_postfix(volatile std::uint64_t* target, std::uint64_t value, int) { return __atomic_fetch_or(target, value, __ATOMIC_SEQ_CST); }

std::uint8_t APS5_VABI __atomic_fetch_xor_1_nid_postfix(volatile std::uint8_t* target, std::uint8_t value, int) { return __atomic_fetch_xor(target, value, __ATOMIC_SEQ_CST); }
std::uint16_t APS5_VABI __atomic_fetch_xor_2_nid_postfix(volatile std::uint16_t* target, std::uint16_t value, int) { return __atomic_fetch_xor(target, value, __ATOMIC_SEQ_CST); }
std::uint32_t APS5_VABI __atomic_fetch_xor_4_nid_postfix(volatile std::uint32_t* target, std::uint32_t value, int) { return __atomic_fetch_xor(target, value, __ATOMIC_SEQ_CST); }
std::uint64_t APS5_VABI __atomic_fetch_xor_8_nid_postfix(volatile std::uint64_t* target, std::uint64_t value, int) { return __atomic_fetch_xor(target, value, __ATOMIC_SEQ_CST); }

}
