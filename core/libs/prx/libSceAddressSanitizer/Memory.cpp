#include "prx/libSceAddressSanitizer/include/Api.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" {

void* APS5_VABI __asan_memcpy(void* destination, const void* source, std::size_t size) {
    return memcpy_nid_postfix(destination, source, size);
}

void* APS5_VABI __asan_memmove(void* destination, const void* source, std::size_t size) {
    return memmove_nid_postfix(destination, source, size);
}

void* APS5_VABI __asan_memset(void* destination, int value, std::size_t size) {
    return memset_nid_postfix(destination, value, size);
}

std::uint16_t APS5_VABI __sanitizer_unaligned_load16(const void* pointer) {
    std::uint16_t value = 0;
    std::memcpy(&value, pointer, sizeof(value));
    return value;
}

std::uint32_t APS5_VABI __sanitizer_unaligned_load32(const void* pointer) {
    std::uint32_t value = 0;
    std::memcpy(&value, pointer, sizeof(value));
    return value;
}

std::uint64_t APS5_VABI __sanitizer_unaligned_load64(const void* pointer) {
    std::uint64_t value = 0;
    std::memcpy(&value, pointer, sizeof(value));
    return value;
}

void APS5_VABI __sanitizer_unaligned_store16(void* pointer, std::uint16_t value) {
    std::memcpy(pointer, &value, sizeof(value));
}

void APS5_VABI __sanitizer_unaligned_store32(void* pointer, std::uint32_t value) {
    std::memcpy(pointer, &value, sizeof(value));
}

void APS5_VABI __sanitizer_unaligned_store64(void* pointer, std::uint64_t value) {
    std::memcpy(pointer, &value, sizeof(value));
}

int APS5_VABI __sanitizer_ptr_cmp(const void* left, const void* right) {
    const auto a = reinterpret_cast<std::uintptr_t>(left);
    const auto b = reinterpret_cast<std::uintptr_t>(right);
    return a < b ? -1 : (a > b ? 1 : 0);
}

std::uintptr_t APS5_VABI __sanitizer_ptr_sub(const void* left, const void* right) {
    return reinterpret_cast<std::uintptr_t>(left) - reinterpret_cast<std::uintptr_t>(right);
}

}
