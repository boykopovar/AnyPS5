#include "prx/libc/include/LibcxxStringSearch.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {

constexpr std::uint64_t GuestNpos = std::numeric_limits<std::uint64_t>::max();
constexpr std::size_t GuestStringSize = 24;
constexpr std::size_t ShortDataOffset = 1;
constexpr std::size_t MaxShortSize = GuestStringSize - ShortDataOffset - 1;
constexpr std::size_t LongSizeOffset = 8;
constexpr std::size_t LongDataOffset = 16;

template <bool Reverse>
std::uint64_t Search(const void* self, char value, std::uint64_t position) {
    std::array<char, GuestStringSize> descriptor{};
    if (!GuestCopyIn_nid_no_patch(descriptor.data(), self, descriptor.size())) {
        throw std::invalid_argument("libc++ string search: unreadable string");
    }

    auto flags = static_cast<unsigned char>(descriptor[0]);
    if ((flags & 1) == 0) {
        auto size = static_cast<std::size_t>(flags >> 1);
        if (size > MaxShortSize) {
            throw std::invalid_argument("libc++ string search: invalid short string size");
        }
        const std::string_view bytes{descriptor.data() + ShortDataOffset, size};
        const auto found = Reverse ? bytes.rfind(value, position) : bytes.find(value, position);
        return found == std::string_view::npos ? GuestNpos : found;
    }

    std::uint64_t size = 0;
    std::uintptr_t address = 0;
    std::memcpy(&size, descriptor.data() + LongSizeOffset, sizeof(size));
    std::memcpy(&address, descriptor.data() + LongDataOffset, sizeof(address));
    if (size == 0 || (!Reverse && position >= size)) {
        return GuestNpos;
    }
    if (address == 0 || size - 1 > std::numeric_limits<std::uintptr_t>::max() - address) {
        throw std::invalid_argument("libc++ string search: invalid long string range");
    }

    std::array<char, 4096> buffer;
    auto first = Reverse ? 0 : position;
    auto last = Reverse ? std::min(position, size - 1) + 1 : size;
    while (first < last) {
        const auto count = std::min<std::uint64_t>(last - first, buffer.size());
        const auto start = Reverse ? last - count : first;
        const auto* data = reinterpret_cast<const void*>(address + start);
        if (!GuestCopyIn_nid_no_patch(buffer.data(), data, count)) {
            throw std::invalid_argument("libc++ string search: unreadable string data");
        }
        const std::string_view bytes{buffer.data(), static_cast<std::size_t>(count)};
        const auto found = Reverse ? bytes.rfind(value) : bytes.find(value);
        if (found != std::string_view::npos) {
            return start + found;
        }
        if constexpr (Reverse) {
            last = start;
        } else {
            first += count;
        }
    }
    return GuestNpos;
}

}

extern "C" std::uint64_t APS5_VABI _ZNKSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE4findEcm_nid_postfix(
    const void* self, char value, std::uint64_t position) {
    return Search<false>(self, value, position);
}

extern "C" std::uint64_t APS5_VABI _ZNKSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE5rfindEcm_nid_postfix(
    const void* self, char value, std::uint64_t position) {
    return Search<true>(self, value, position);
}
