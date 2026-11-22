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

void CopyLongStorage(char* destination, const char* source, std::size_t bytes) {
    std::array<char, 4096> buffer{};
    for (std::size_t offset = 0; offset < bytes;) {
        const auto count = std::min(buffer.size(), bytes - offset);
        if (!GuestCopyIn_nid_no_patch(buffer.data(), source + offset, count) ||
            !GuestCopyOut_nid_no_patch(destination + offset, buffer.data(), count)) {
            _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string::reserve: inaccessible string storage");
        }
        offset += count;
    }
}

}

extern "C" void APS5_VABI _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7reserveEm_nid_postfix(
    void* string, std::uint64_t requested) {
    if (requested > UINT64_MAX - AllocationAlignment - 1u) {
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

    const auto allocatedBytes = (requested + AllocationAlignment) & ~(AllocationAlignment - 1u);
    if (allocatedBytes > SIZE_MAX) throw std::bad_alloc();
    std::unique_ptr<void, decltype(&_ZdlPv_nid_postfix)> allocation(
        _Znwm_nid_postfix(static_cast<std::size_t>(allocatedBytes)), &_ZdlPv_nid_postfix);
    auto* data = static_cast<char*>(allocation.get());
    const auto bytesToCopy = static_cast<std::size_t>(size + 1u);
    if (previous) {
        CopyLongStorage(data, previous, bytesToCopy);
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
