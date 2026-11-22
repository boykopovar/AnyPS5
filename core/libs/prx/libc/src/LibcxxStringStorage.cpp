#include "prx/libc/include/Api.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <memory>
#include <new>

namespace {

using Representation = std::array<unsigned char, 24>;

struct LongString {
    std::uint64_t taggedCapacity;
    std::uint64_t size;
    std::uint64_t address;
};

static_assert(sizeof(LongString) == sizeof(Representation));

constexpr std::uint64_t InlineCapacity = sizeof(Representation) - 2;
constexpr std::uint64_t LongFlag = 1u;
constexpr std::uint64_t AllocationAlignment = 16u;
constexpr std::uint64_t MaxSize = UINT64_MAX - AllocationAlignment - 1u;
constexpr char AppendInaccessible[] = "basic_string::append: inaccessible string storage";
using GuestAllocation = std::unique_ptr<void, decltype(&_ZdlPv_nid_postfix)>;

Representation ReadString(const void* string) {
    Representation result{};
    if (!GuestCopyIn_nid_no_patch(result.data(), string, result.size())) {
        _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string: unreadable representation");
    }
    return result;
}

LongString ReadLongString(const Representation& representation) {
    const auto fields = std::bit_cast<LongString>(representation);
    if ((fields.taggedCapacity & ~LongFlag) <= fields.size || fields.address == 0 ||
        fields.address > UINTPTR_MAX || fields.size >= UINTPTR_MAX - fields.address) {
        _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string: invalid long representation");
    }
    return fields;
}

void CopyLongStorage(char* destination, const char* source, std::size_t bytes, const char* failure) {
    std::array<char, 4096> buffer{};
    const auto from = reinterpret_cast<std::uintptr_t>(source);
    const auto to = reinterpret_cast<std::uintptr_t>(destination);
    const bool backwards = to > from && to - from < bytes;
    for (std::size_t copied = 0; copied < bytes;) {
        const auto count = std::min(buffer.size(), bytes - copied);
        const auto offset = backwards ? bytes - copied - count : copied;
        if (!GuestCopyIn_nid_no_patch(buffer.data(), source + offset, count) ||
            !GuestCopyOut_nid_no_patch(destination + offset, buffer.data(), count)) {
            _ZSt18_Xinvalid_argumentPKc_nid_postfix(failure);
        }
        copied += count;
    }
}

std::uint64_t AllocationSize(std::uint64_t capacity) {
    return (capacity + AllocationAlignment) & ~(AllocationAlignment - 1u);
}

GuestAllocation AllocateLongStorage(std::uint64_t bytes) {
    if (bytes > SIZE_MAX) throw std::bad_alloc();
    return GuestAllocation(_Znwm_nid_postfix(static_cast<std::size_t>(bytes)), &_ZdlPv_nid_postfix);
}

void WriteAppendBytes(char* destination, const char* source, std::size_t count, char value) {
    if (source) {
        CopyLongStorage(destination, source, count, AppendInaccessible);
    } else {
        std::array<char, 4096> buffer;
        buffer.fill(value);
        for (std::size_t written = 0; written < count;) {
            const auto step = std::min(buffer.size(), count - written);
            if (!GuestCopyOut_nid_no_patch(destination + written, buffer.data(), step)) {
                _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string::append: unwritable string storage");
            }
            written += step;
        }
    }
    constexpr char terminator = '\0';
    if (!GuestCopyOut_nid_no_patch(destination + count, &terminator, sizeof(terminator))) {
        _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string::append: unwritable string terminator");
    }
}

void* Append(void* string, const char* source, std::uint64_t count, char value = '\0') {
    if (!count) return string;
    auto representation = ReadString(string);
    const bool isLong = (representation[0] & LongFlag) != 0;
    const auto fields = isLong ? ReadLongString(representation) : LongString{};
    const std::uint64_t size = isLong ? fields.size : representation[0] >> 1u;
    if (!isLong && size > InlineCapacity) {
        _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string: invalid short size");
    }
    if (size > MaxSize || count > MaxSize - size) _ZSt14_Xlength_errorPKc_nid_postfix("basic_string");
    const auto nextSize = size + count;
    if (nextSize >= SIZE_MAX) throw std::bad_alloc();
    const auto capacity = isLong ? (fields.taggedCapacity & ~LongFlag) - 1u : InlineCapacity;
    auto* previous = reinterpret_cast<char*>(static_cast<std::uintptr_t>(fields.address));
    if (!isLong && nextSize <= InlineCapacity) {
        auto* destination = representation.data() + 1 + size;
        if (source) {
            if (!GuestCopyIn_nid_no_patch(destination, source, static_cast<std::size_t>(count))) {
                _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string::append: unreadable source");
            }
        } else {
            std::fill_n(destination, count, static_cast<unsigned char>(value));
        }
        representation[0] = static_cast<unsigned char>(nextSize << 1u);
        representation[1 + nextSize] = 0;
    } else if (isLong && nextSize <= capacity) {
        if (nextSize >= UINTPTR_MAX - fields.address || !GuestWritable_nid_no_patch(string, representation.size())) {
            _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string::append: unwritable representation");
        }
        WriteAppendBytes(previous + size, source, static_cast<std::size_t>(count), value);
        representation = std::bit_cast<Representation>(LongString{fields.taggedCapacity, nextSize, fields.address});
    } else {
        const auto grown = capacity <= MaxSize / 2u ? capacity * 2u : MaxSize;
        const auto allocated = AllocationSize(std::max(grown, nextSize));
        auto allocation = AllocateLongStorage(allocated);
        auto* data = static_cast<char*>(allocation.get());
        if (isLong) {
            CopyLongStorage(data, previous, static_cast<std::size_t>(size), AppendInaccessible);
        } else if (!GuestCopyOut_nid_no_patch(data, representation.data() + 1, static_cast<std::size_t>(size))) {
            _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string::append: unwritable string storage");
        }
        WriteAppendBytes(data + size, source, static_cast<std::size_t>(count), value);
        const LongString replacement{allocated | LongFlag, nextSize, reinterpret_cast<std::uintptr_t>(data)};
        if (!GuestCopyOut_nid_no_patch(string, &replacement, sizeof(replacement))) {
            _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string::append: unwritable representation");
        }
        allocation.release();
        _ZdlPv_nid_postfix(previous);
        return string;
    }
    if (!GuestCopyOut_nid_no_patch(string, representation.data(), representation.size())) {
        _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string::append: unwritable representation");
    }
    return string;
}

}

extern "C" void APS5_VABI _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7reserveEm_nid_postfix(
    void* string, std::uint64_t requested) {
    if (requested > MaxSize) {
        _ZSt14_Xlength_errorPKc_nid_postfix("basic_string");
    }

    const auto representation = ReadString(string);
    const bool isLong = (representation[0] & LongFlag) != 0;
    std::uint64_t size = representation[0] >> 1u;
    auto capacity = InlineCapacity;
    char* previous = nullptr;
    if (isLong) {
        const auto fields = ReadLongString(representation);
        size = fields.size;
        capacity = (fields.taggedCapacity & ~LongFlag) - 1u;
        previous = reinterpret_cast<char*>(static_cast<std::uintptr_t>(fields.address));
    } else if (size > InlineCapacity) {
        _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string: invalid short size");
    }
    if (requested <= capacity) return;

    const auto allocatedBytes = AllocationSize(requested);
    auto allocation = AllocateLongStorage(allocatedBytes);
    auto* data = static_cast<char*>(allocation.get());
    const auto bytesToCopy = static_cast<std::size_t>(size + 1u);
    if (previous) {
        CopyLongStorage(data, previous, bytesToCopy, "basic_string::reserve: inaccessible string storage");
    } else if (!GuestCopyOut_nid_no_patch(data, representation.data() + 1, bytesToCopy)) {
        _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string::reserve: unwritable string storage");
    }

    const LongString replacement{allocatedBytes | LongFlag, size, reinterpret_cast<std::uintptr_t>(data)};
    if (!GuestCopyOut_nid_no_patch(string, &replacement, sizeof(replacement))) {
        _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string::reserve: unwritable representation");
    }
    allocation.release();
    _ZdlPv_nid_postfix(previous);
}

extern "C" void APS5_VABI _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEED2Ev_nid_postfix(void* string) {
    const auto representation = ReadString(string);
    if ((representation[0] & LongFlag) == 0) return;
    const auto fields = ReadLongString(representation);
    _ZdlPv_nid_postfix(reinterpret_cast<void*>(static_cast<std::uintptr_t>(fields.address)));
}

extern "C" void* APS5_VABI _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6appendEPKc_nid_postfix(
    void* string, const char* source) {
    if (!source) _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string::append: null source");
    return Append(string, source, strlen_nid_postfix(source));
}

extern "C" void* APS5_VABI _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6appendEPKcm_nid_postfix(
    void* string, const char* source, std::uint64_t count) {
    if (count && !source) _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string::append: null source");
    return Append(string, source, count);
}

extern "C" void* APS5_VABI _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6appendEmc_nid_postfix(
    void* string, std::uint64_t count, char value) {
    return Append(string, nullptr, count, value);
}
