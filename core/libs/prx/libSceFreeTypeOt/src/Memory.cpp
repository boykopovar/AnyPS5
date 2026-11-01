#include "prx/libSceFreeTypeOt/include/Memory.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

constexpr std::int32_t InvalidArgument = 0x06;
constexpr std::int32_t ArrayTooLarge = 0x0A;
constexpr std::int32_t OutOfMemory = 0x40;
constexpr std::int64_t IntMax = 0x7FFFFFFF;

std::int64_t Bytes(std::int64_t count, std::int64_t itemSize) {
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(count) * static_cast<std::uint64_t>(itemSize));
}

}

extern "C" {

void* APS5_VABI ft_mem_qalloc_nid_postfix(FreeType::GuestMemoryRec* memory, std::int64_t size, std::int32_t* error) {
    std::int32_t result = 0;
    void* block = nullptr;
    if (size > 0) {
        block = memory->alloc(memory, size);
        if (!block) result = OutOfMemory;
    } else if (size < 0) {
        result = InvalidArgument;
    }
    *error = result;
    return block;
}

void* APS5_VABI ft_mem_alloc_nid_postfix(FreeType::GuestMemoryRec* memory, std::int64_t size, std::int32_t* error) {
    std::int32_t result = 0;
    void* block = ft_mem_qalloc_nid_postfix(memory, size, &result);
    if (!result && block && size > 0) std::memset(block, 0, static_cast<std::size_t>(size));
    *error = result;
    return block;
}

void APS5_VABI ft_mem_free_nid_postfix(FreeType::GuestMemoryRec* memory, const void* block) {
    if (block) memory->free(memory, const_cast<void*>(block));
}

void* APS5_VABI ft_mem_qrealloc_nid_postfix(FreeType::GuestMemoryRec* memory, std::int64_t itemSize, std::int64_t currentCount,
                                            std::int64_t newCount, void* block, std::int32_t* error) {
    std::int32_t result = 0;
    if (currentCount < 0 || newCount < 0 || itemSize < 0) {
        result = InvalidArgument;
    } else if (newCount == 0 || itemSize == 0) {
        ft_mem_free_nid_postfix(memory, block);
        block = nullptr;
    } else if (newCount > IntMax / itemSize) {
        result = ArrayTooLarge;
    } else if (currentCount == 0) {
        block = ft_mem_alloc_nid_postfix(memory, newCount * itemSize, &result);
    } else {
        void* resized = memory->realloc(memory, Bytes(currentCount, itemSize), newCount * itemSize, block);
        if (resized) {
            block = resized;
        } else {
            result = OutOfMemory;
        }
    }
    *error = result;
    return block;
}

void* APS5_VABI ft_mem_realloc_nid_postfix(FreeType::GuestMemoryRec* memory, std::int64_t itemSize, std::int64_t currentCount,
                                           std::int64_t newCount, void* block, std::int32_t* error) {
    std::int32_t result = 0;
    block = ft_mem_qrealloc_nid_postfix(memory, itemSize, currentCount, newCount, block, &result);
    if (!result && block && newCount > currentCount) {
        std::memset(static_cast<char*>(block) + currentCount * itemSize, 0, static_cast<std::size_t>((newCount - currentCount) * itemSize));
    }
    *error = result;
    return block;
}

void* APS5_VABI ft_mem_dup_nid_postfix(FreeType::GuestMemoryRec* memory, const void* address, std::uint64_t size, std::int32_t* error) {
    std::int32_t result = 0;
    void* copy = ft_mem_qalloc_nid_postfix(memory, static_cast<std::int64_t>(size), &result);
    if (!result && address && size > 0) std::memcpy(copy, address, static_cast<std::size_t>(size));
    *error = result;
    return copy;
}

void* APS5_VABI ft_mem_strdup_nid_postfix(FreeType::GuestMemoryRec* memory, const char* string, std::int32_t* error) {
    const std::uint64_t length = string ? std::strlen(string) + 1 : 0;
    return ft_mem_dup_nid_postfix(memory, string, length, error);
}

std::int32_t APS5_VABI ft_mem_strcpyn_nid_postfix(char* destination, const char* source, std::uint64_t size) {
    while (size > 1 && *source != 0) {
        *destination++ = *source++;
        --size;
    }
    *destination = 0;
    return *source != 0;
}

}
