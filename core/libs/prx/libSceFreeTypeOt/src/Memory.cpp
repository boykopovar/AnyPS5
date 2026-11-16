#include "prx/libSceFreeTypeOt/include/Memory.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>

namespace {

constexpr std::int32_t InvalidArgument = 0x06;
constexpr std::int32_t ArrayTooLarge = 0x0A;
constexpr std::int32_t OutOfMemory = 0x40;
constexpr std::int64_t IntMax = 0x7FFFFFFF;
constexpr std::size_t Chunk = 4096;

std::int64_t Bytes(std::int64_t count, std::int64_t itemSize) {
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(count) * static_cast<std::uint64_t>(itemSize));
}

void CopyIn(void* destination, const void* source, std::size_t bytes, const char* caller) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
}

void CopyOut(void* destination, const void* source, std::size_t bytes, const char* caller) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
}

void RequireWritable(void* destination, std::size_t bytes, const char* caller) {
    const int savedErrno = errno;
    const bool writable = GuestWritable_nid_no_patch(destination, bytes);
    errno = savedErrno;
    if (!writable) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
}

FreeType::GuestMemoryRec ReadMemory(FreeType::GuestMemoryRec* memory, const char* caller) {
    FreeType::GuestMemoryRec record{};
    CopyIn(&record, memory, sizeof(record), caller);
    return record;
}

void Zero(void* block, std::size_t bytes, const char* caller) {
    static constexpr std::array<char, Chunk> zeros{};
    auto* destination = static_cast<char*>(block);
    for (std::size_t offset = 0; offset < bytes; offset += Chunk) {
        CopyOut(destination + offset, zeros.data(), std::min(Chunk, bytes - offset), caller);
    }
}

void Copy(void* destination, const void* source, std::size_t bytes, const char* caller) {
    std::array<char, Chunk> buffer;
    for (std::size_t offset = 0; offset < bytes; offset += Chunk) {
        const std::size_t count = std::min(Chunk, bytes - offset);
        CopyIn(buffer.data(), static_cast<const char*>(source) + offset, count, caller);
        CopyOut(static_cast<char*>(destination) + offset, buffer.data(), count, caller);
    }
}

std::string ReadText(const char* source, std::size_t limit, const char* caller) {
    std::string text;
    std::array<char, Chunk> buffer;
    auto address = reinterpret_cast<std::uintptr_t>(source);
    while (text.size() < limit) {
        const std::size_t count = std::min(Chunk - address % Chunk, limit - text.size());
        CopyIn(buffer.data(), reinterpret_cast<const void*>(address), count, caller);
        const auto* end = static_cast<const char*>(std::memchr(buffer.data(), 0, count));
        text.append(buffer.data(), end ? static_cast<std::size_t>(end - buffer.data()) : count);
        if (end) break;
        address += count;
    }
    return text;
}

void* QuickAllocate(FreeType::GuestMemoryRec* memory, std::int64_t size, std::int32_t& result, const char* caller) {
    result = 0;
    void* block = nullptr;
    if (size > 0) {
        block = ReadMemory(memory, caller).alloc(memory, size);
        if (!block) result = OutOfMemory;
    } else if (size < 0) {
        result = InvalidArgument;
    }
    return block;
}

void* Allocate(FreeType::GuestMemoryRec* memory, std::int64_t size, std::int32_t& result, const char* caller) {
    void* block = QuickAllocate(memory, size, result, caller);
    if (!result && block && size > 0) Zero(block, static_cast<std::size_t>(size), caller);
    return block;
}

void Free(FreeType::GuestMemoryRec* memory, const void* block, const char* caller) {
    if (block) ReadMemory(memory, caller).free(memory, const_cast<void*>(block));
}

void* QuickReallocate(FreeType::GuestMemoryRec* memory, std::int64_t itemSize, std::int64_t currentCount, std::int64_t newCount,
                      void* block, std::int32_t& result, const char* caller) {
    result = 0;
    if (currentCount < 0 || newCount < 0 || itemSize < 0) {
        result = InvalidArgument;
    } else if (newCount == 0 || itemSize == 0) {
        Free(memory, block, caller);
        block = nullptr;
    } else if (newCount > IntMax / itemSize) {
        result = ArrayTooLarge;
    } else if (currentCount == 0) {
        block = Allocate(memory, newCount * itemSize, result, caller);
    } else {
        void* resized = ReadMemory(memory, caller).realloc(memory, Bytes(currentCount, itemSize), newCount * itemSize, block);
        if (resized) {
            block = resized;
        } else {
            result = OutOfMemory;
        }
    }
    return block;
}

void* Duplicate(FreeType::GuestMemoryRec* memory, const void* address, std::uint64_t size, std::int32_t* error, const char* caller) {
    RequireWritable(error, sizeof(*error), caller);
    std::int32_t result = 0;
    void* copy = QuickAllocate(memory, static_cast<std::int64_t>(size), result, caller);
    if (!result && address && size > 0) Copy(copy, address, static_cast<std::size_t>(size), caller);
    CopyOut(error, &result, sizeof(result), caller);
    return copy;
}

}

extern "C" {

void* APS5_VABI ft_mem_qalloc_nid_postfix(FreeType::GuestMemoryRec* memory, std::int64_t size, std::int32_t* error) {
    RequireWritable(error, sizeof(*error), __func__);
    std::int32_t result = 0;
    void* block = QuickAllocate(memory, size, result, __func__);
    CopyOut(error, &result, sizeof(result), __func__);
    return block;
}

void* APS5_VABI ft_mem_alloc_nid_postfix(FreeType::GuestMemoryRec* memory, std::int64_t size, std::int32_t* error) {
    RequireWritable(error, sizeof(*error), __func__);
    std::int32_t result = 0;
    void* block = Allocate(memory, size, result, __func__);
    CopyOut(error, &result, sizeof(result), __func__);
    return block;
}

void APS5_VABI ft_mem_free_nid_postfix(FreeType::GuestMemoryRec* memory, const void* block) {
    Free(memory, block, __func__);
}

void* APS5_VABI ft_mem_qrealloc_nid_postfix(FreeType::GuestMemoryRec* memory, std::int64_t itemSize, std::int64_t currentCount,
                                            std::int64_t newCount, void* block, std::int32_t* error) {
    RequireWritable(error, sizeof(*error), __func__);
    std::int32_t result = 0;
    block = QuickReallocate(memory, itemSize, currentCount, newCount, block, result, __func__);
    CopyOut(error, &result, sizeof(result), __func__);
    return block;
}

void* APS5_VABI ft_mem_realloc_nid_postfix(FreeType::GuestMemoryRec* memory, std::int64_t itemSize, std::int64_t currentCount,
                                           std::int64_t newCount, void* block, std::int32_t* error) {
    RequireWritable(error, sizeof(*error), __func__);
    std::int32_t result = 0;
    block = QuickReallocate(memory, itemSize, currentCount, newCount, block, result, __func__);
    if (!result && block && newCount > currentCount) {
        Zero(static_cast<char*>(block) + currentCount * itemSize, static_cast<std::size_t>((newCount - currentCount) * itemSize), __func__);
    }
    CopyOut(error, &result, sizeof(result), __func__);
    return block;
}

void* APS5_VABI ft_mem_dup_nid_postfix(FreeType::GuestMemoryRec* memory, const void* address, std::uint64_t size, std::int32_t* error) {
    return Duplicate(memory, address, size, error, __func__);
}

void* APS5_VABI ft_mem_strdup_nid_postfix(FreeType::GuestMemoryRec* memory, const char* string, std::int32_t* error) {
    const std::uint64_t length = string ? ReadText(string, std::numeric_limits<std::size_t>::max(), __func__).size() + 1 : 0;
    return Duplicate(memory, string, length, error, __func__);
}

std::int32_t APS5_VABI ft_mem_strcpyn_nid_postfix(char* destination, const char* source, std::uint64_t size) {
    const std::size_t limit = size > 1 ? static_cast<std::size_t>(size - 1) : 0;
    std::string text = ReadText(source, limit + 1, __func__);
    const bool truncated = text.size() > limit;
    if (truncated) text.resize(limit);
    CopyOut(destination, text.c_str(), text.size() + 1, __func__);
    return truncated;
}

}
