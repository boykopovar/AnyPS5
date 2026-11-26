#include "prx/libc/include/Api.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <memory>

namespace {

using Representation = std::array<char, 24>;

struct LongString {
    std::uint64_t taggedCapacity;
    std::uint64_t size;
    std::uint64_t address;
};

static_assert(sizeof(LongString) == sizeof(Representation));

constexpr std::uint64_t LongFlag = 1u;
constexpr std::uint64_t MinimumCapacity = sizeof(Representation) - 1;
constexpr std::uint64_t Alignment = 16u;
constexpr std::uint64_t MaximumSize = UINT64_MAX - Alignment;

using Allocation = std::unique_ptr<void, decltype(&_ZdlPv_nid_postfix)>;

struct GuestString {
    Representation representation{};
    bool isLong = false;
    std::uint64_t size = 0;
    std::uint64_t allocation = MinimumCapacity;
    char* data = nullptr;
};

GuestString ReadString(const void* string) {
    GuestString result;
    if (!GuestCopyIn_nid_no_patch(result.representation.data(), string, result.representation.size())) {
        _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string: unreadable representation");
    }
    const auto flags = static_cast<unsigned char>(result.representation[0]);
    result.isLong = (flags & LongFlag) != 0;
    if (!result.isLong) {
        result.size = flags >> 1u;
        if (result.size >= MinimumCapacity) {
            _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string: invalid short size");
        }
        return result;
    }
    const auto fields = std::bit_cast<LongString>(result.representation);
    result.size = fields.size;
    result.allocation = fields.taggedCapacity & ~LongFlag;
    if (result.allocation <= fields.size || fields.address == 0 || fields.address > UINTPTR_MAX ||
        fields.size >= UINTPTR_MAX - fields.address) {
        _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string: invalid long representation");
    }
    result.data = reinterpret_cast<char*>(static_cast<std::uintptr_t>(fields.address));
    return result;
}

std::uint64_t Recommend(std::uint64_t size) {
    if (size < MinimumCapacity) return MinimumCapacity - 1;
    return ((size + Alignment) & ~(Alignment - 1u)) - 1u;
}

std::uint64_t GrownCapacity(std::uint64_t oldCapacity, std::uint64_t deltaCapacity) {
    return oldCapacity < MaximumSize / 2 - Alignment
               ? Recommend(std::max(oldCapacity + deltaCapacity, 2 * oldCapacity))
               : MaximumSize - 1;
}

Allocation Allocate(std::uint64_t capacity) {
    return Allocation(_Znwm_nid_postfix(static_cast<std::size_t>(capacity + 1u)), &_ZdlPv_nid_postfix);
}

void CopyGuest(char* destination, const char* source, std::uint64_t bytes) {
    std::array<char, 4096> block{};
    for (std::uint64_t copied = 0; copied < bytes;) {
        const auto count = std::min<std::uint64_t>(block.size(), bytes - copied);
        if (!GuestCopyIn_nid_no_patch(block.data(), source + copied, count) ||
            !GuestCopyOut_nid_no_patch(destination + copied, block.data(), count)) {
            _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string: inaccessible string storage");
        }
        copied += count;
    }
}

void CopyContents(char* destination, const GuestString& source, std::uint64_t offset, std::uint64_t bytes) {
    if (bytes == 0) return;
    if (source.isLong) {
        CopyGuest(destination, source.data + offset, bytes);
    } else if (!GuestCopyOut_nid_no_patch(destination, source.representation.data() + 1 + offset, bytes)) {
        _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string: unwritable string storage");
    }
}

void ReadContents(char* destination, const GuestString& source) {
    if (!source.isLong) {
        std::memcpy(destination, source.representation.data() + 1, source.size);
    } else if (!GuestCopyIn_nid_no_patch(destination, source.data, source.size)) {
        _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string: unreadable string storage");
    }
}

void WriteTerminator(char* destination) {
    constexpr char terminator = '\0';
    if (!GuestCopyOut_nid_no_patch(destination, &terminator, sizeof(terminator))) {
        _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string: unwritable string storage");
    }
}

void WriteRepresentation(void* string, const void* representation, std::size_t bytes) {
    if (!GuestCopyOut_nid_no_patch(string, representation, bytes)) {
        _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string: unwritable representation");
    }
}

void InstallLong(void* string, Representation representation, std::uint64_t capacity, Allocation& allocation) {
    auto fields = std::bit_cast<LongString>(representation);
    fields.taggedCapacity = ((capacity + 1u) & ~LongFlag) | LongFlag;
    fields.address = reinterpret_cast<std::uintptr_t>(allocation.get());
    WriteRepresentation(string, &fields, sizeof(fields));
    allocation.release();
}

std::uint64_t GuestLength(const char* text) {
    if (text == nullptr) _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string: unreadable character array");
    std::array<char, 4096> block{};
    const auto address = reinterpret_cast<std::uintptr_t>(text);
    for (std::uint64_t length = 0;;) {
        const auto count = block.size() - (address + length) % block.size();
        if (!GuestCopyIn_nid_no_patch(block.data(), text + length, count)) {
            _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string: unreadable character array");
        }
        if (const auto* end = static_cast<const char*>(std::memchr(block.data(), 0, count))) {
            return length + static_cast<std::uint64_t>(end - block.data());
        }
        length += count;
    }
}

}

extern "C" void APS5_VABI _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE9__grow_byEmmmmmm_nid_postfix(
    void* string, std::uint64_t oldCapacity, std::uint64_t deltaCapacity, std::uint64_t oldSize,
    std::uint64_t copied, std::uint64_t deleted, std::uint64_t added) {
    if (deltaCapacity > MaximumSize - oldCapacity) _ZSt14_Xlength_errorPKc_nid_postfix("basic_string");
    const auto current = ReadString(string);
    const auto capacity = GrownCapacity(oldCapacity, deltaCapacity);
    if (oldCapacity + 1u != current.allocation || oldSize > oldCapacity || copied > oldSize ||
        deleted > oldSize - copied) {
        _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string::__grow_by: invalid arguments");
    }
    const auto tail = oldSize - deleted - copied;
    if (copied + tail > capacity || added > capacity - copied - tail) {
        _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string::__grow_by: invalid arguments");
    }

    auto allocation = Allocate(capacity);
    auto* data = static_cast<char*>(allocation.get());
    CopyContents(data, current, 0, copied);
    CopyContents(data + copied + added, current, copied + deleted, tail);
    InstallLong(string, current.representation, capacity, allocation);
    if (oldCapacity + 1u != MinimumCapacity) _ZdlPv_nid_postfix(current.data);
}

extern "C" void* APS5_VABI _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEaSERKS5__nid_postfix(
    void* string, const void* source) {
    if (string == source) return string;
    auto target = ReadString(string);
    const auto from = ReadString(source);
    if (!target.isLong && !from.isLong) {
        WriteRepresentation(string, from.representation.data(), from.representation.size());
        return string;
    }

    const auto size = from.size;
    if (size < target.allocation) {
        if (!target.isLong) {
            target.representation[0] = static_cast<char>(size << 1u);
            ReadContents(target.representation.data() + 1, from);
            target.representation[size + 1] = '\0';
            WriteRepresentation(string, target.representation.data(), size + 2);
            return string;
        }
        CopyContents(target.data, from, 0, size);
        WriteTerminator(target.data + size);
        WriteRepresentation(static_cast<char*>(string) + 8, &size, sizeof(size));
        return string;
    }

    const auto oldCapacity = target.allocation - 1u;
    const auto deltaCapacity = size - oldCapacity;
    if (deltaCapacity > MaximumSize - oldCapacity - 1u) _ZSt14_Xlength_errorPKc_nid_postfix("basic_string");
    const auto capacity = GrownCapacity(oldCapacity, deltaCapacity);
    auto allocation = Allocate(capacity);
    auto* data = static_cast<char*>(allocation.get());
    CopyContents(data, from, 0, size);
    WriteTerminator(data + size);
    auto fields = std::bit_cast<LongString>(target.representation);
    fields.size = size;
    InstallLong(string, std::bit_cast<Representation>(fields), capacity, allocation);
    if (target.allocation != MinimumCapacity) _ZdlPv_nid_postfix(target.data);
    return string;
}

extern "C" void* APS5_VABI _ZNSt3__1plIcNS_11char_traitsIcEENS_9allocatorIcEEEENS_12basic_stringIT_T0_T1_EEPKS6_RKS9__nid_postfix(
    void* result, const char* left, const void* right) {
    const auto leftSize = GuestLength(left);
    const auto suffix = ReadString(right);
    if (suffix.size > MaximumSize || leftSize > MaximumSize - suffix.size) {
        _ZSt14_Xlength_errorPKc_nid_postfix("basic_string");
    }
    const auto size = leftSize + suffix.size;
    Representation representation{};
    if (size < MinimumCapacity) {
        representation[0] = static_cast<char>(size << 1u);
        if (!GuestCopyIn_nid_no_patch(representation.data() + 1, left, leftSize)) {
            _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string: unreadable character array");
        }
        ReadContents(representation.data() + 1 + leftSize, suffix);
        WriteRepresentation(result, representation.data(), representation.size());
        return result;
    }

    const auto capacity = Recommend(size);
    auto allocation = Allocate(capacity);
    auto* data = static_cast<char*>(allocation.get());
    CopyGuest(data, left, leftSize);
    CopyContents(data + leftSize, suffix, 0, suffix.size);
    WriteTerminator(data + size);
    const LongString fields{0, size, 0};
    InstallLong(result, std::bit_cast<Representation>(fields), capacity, allocation);
    return result;
}
