#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "prx/libc/include/General.hpp"

namespace {

template<typename TValue>
TValue Read(const unsigned char* address) {
    TValue value;
    std::memcpy(&value, address, sizeof(value));
    return value;
}

template<typename TValue>
const void* Find(const void* first, const void* last, TValue value) {
    auto* current = static_cast<const unsigned char*>(first);
    auto* end = static_cast<const unsigned char*>(last);
    for (; current != end; current += sizeof(TValue)) {
        if (Read<TValue>(current) == value) return current;
    }
    return end;
}

constexpr std::uintptr_t UnsizedBlock = 32;

template<typename TValue>
const void* FindUnsized(const void* first, TValue value) {
    auto* start = static_cast<const unsigned char*>(first);
    auto* block = start - (reinterpret_cast<std::uintptr_t>(start) & (UnsizedBlock - 1));
    for (auto* current = start; current != block + UnsizedBlock; current += sizeof(TValue)) {
        if (Read<TValue>(current) == value) return current;
    }
    auto* current = block;
    while (Read<TValue>(current) != value) current += sizeof(TValue);
    return current;
}

template<typename TValue>
std::size_t Count(const void* first, const void* last, TValue value) {
    auto* current = static_cast<const unsigned char*>(first);
    auto* end = static_cast<const unsigned char*>(last);
    std::size_t count = 0;
    for (; current != end; current += sizeof(TValue)) {
        if (Read<TValue>(current) == value) ++count;
    }
    return count;
}

template<typename TValue>
MinMaxPointers MinMax(const void* first, const void* last, bool wantMinimum, bool wantMaximum) {
    auto* current = static_cast<const unsigned char*>(first);
    auto* end = static_cast<const unsigned char*>(last);
    MinMaxPointers result{first, first};
    if (current == end) return result;
    TValue minimum = Read<TValue>(current);
    TValue maximum = minimum;
    for (current += sizeof(TValue); current != end; current += sizeof(TValue)) {
        const TValue value = Read<TValue>(current);
        if (wantMinimum && value < minimum) {
            minimum = value;
            result.minimum = current;
        }
        if (wantMaximum && (wantMinimum ? !(value < maximum) : maximum < value)) {
            maximum = value;
            result.maximum = current;
        }
    }
    return result;
}

template<typename TUnsigned>
MinMaxPointers MinMaxBySign(const void* first, const void* last, bool isSigned, bool wantMinimum, bool wantMaximum) {
    using TSigned = std::make_signed_t<TUnsigned>;
    return isSigned ? MinMax<TSigned>(first, last, wantMinimum, wantMaximum) : MinMax<TUnsigned>(first, last, wantMinimum, wantMaximum);
}

template<std::size_t Size>
void Reverse(void* first, void* last) {
    auto* low = static_cast<unsigned char*>(first);
    auto* high = static_cast<unsigned char*>(last);
    unsigned char buffer[Size];
    while (high - low >= static_cast<std::ptrdiff_t>(2 * Size)) {
        high -= Size;
        std::memcpy(buffer, low, Size);
        std::memcpy(low, high, Size);
        std::memcpy(high, buffer, Size);
        low += Size;
    }
}

template<std::size_t Size>
void ReverseCopy(const void* first, const void* last, void* destination) {
    auto* begin = static_cast<const unsigned char*>(first);
    auto* end = static_cast<const unsigned char*>(last);
    auto* out = static_cast<unsigned char*>(destination);
    while (end != begin) {
        end -= Size;
        std::memcpy(out, end, Size);
        out += Size;
    }
}

}

extern "C" {

const void* APS5_VABI __std_find_trivial_1_nid_postfix(const void* first, const void* last, std::uint8_t value) { return Find(first, last, value); }
const void* APS5_VABI __std_find_trivial_2_nid_postfix(const void* first, const void* last, std::uint16_t value) { return Find(first, last, value); }
const void* APS5_VABI __std_find_trivial_4_nid_postfix(const void* first, const void* last, std::uint32_t value) { return Find(first, last, value); }
const void* APS5_VABI __std_find_trivial_8_nid_postfix(const void* first, const void* last, std::uint64_t value) { return Find(first, last, value); }
const void* APS5_VABI __std_find_trivial_unsized_1_nid_postfix(const void* first, std::uint8_t value) { return FindUnsized(first, value); }
const void* APS5_VABI __std_find_trivial_unsized_2_nid_postfix(const void* first, std::uint16_t value) { return FindUnsized(first, value); }
const void* APS5_VABI __std_find_trivial_unsized_4_nid_postfix(const void* first, std::uint32_t value) { return FindUnsized(first, value); }
const void* APS5_VABI __std_find_trivial_unsized_8_nid_postfix(const void* first, std::uint64_t value) { return FindUnsized(first, value); }
std::size_t APS5_VABI __std_count_trivial_1_nid_postfix(const void* first, const void* last, std::uint8_t value) { return Count(first, last, value); }
std::size_t APS5_VABI __std_count_trivial_2_nid_postfix(const void* first, const void* last, std::uint16_t value) { return Count(first, last, value); }
std::size_t APS5_VABI __std_count_trivial_4_nid_postfix(const void* first, const void* last, std::uint32_t value) { return Count(first, last, value); }
std::size_t APS5_VABI __std_count_trivial_8_nid_postfix(const void* first, const void* last, std::uint64_t value) { return Count(first, last, value); }

const void* APS5_VABI __std_min_element_1_nid_postfix(const void* first, const void* last, bool isSigned) { return MinMaxBySign<std::uint8_t>(first, last, isSigned, true, false).minimum; }
const void* APS5_VABI __std_min_element_2_nid_postfix(const void* first, const void* last, bool isSigned) { return MinMaxBySign<std::uint16_t>(first, last, isSigned, true, false).minimum; }
const void* APS5_VABI __std_min_element_4_nid_postfix(const void* first, const void* last, bool isSigned) { return MinMaxBySign<std::uint32_t>(first, last, isSigned, true, false).minimum; }
const void* APS5_VABI __std_min_element_8_nid_postfix(const void* first, const void* last, bool isSigned) { return MinMaxBySign<std::uint64_t>(first, last, isSigned, true, false).minimum; }
const void* APS5_VABI __std_max_element_1_nid_postfix(const void* first, const void* last, bool isSigned) { return MinMaxBySign<std::uint8_t>(first, last, isSigned, false, true).maximum; }
const void* APS5_VABI __std_max_element_2_nid_postfix(const void* first, const void* last, bool isSigned) { return MinMaxBySign<std::uint16_t>(first, last, isSigned, false, true).maximum; }
const void* APS5_VABI __std_max_element_4_nid_postfix(const void* first, const void* last, bool isSigned) { return MinMaxBySign<std::uint32_t>(first, last, isSigned, false, true).maximum; }
const void* APS5_VABI __std_max_element_8_nid_postfix(const void* first, const void* last, bool isSigned) { return MinMaxBySign<std::uint64_t>(first, last, isSigned, false, true).maximum; }
MinMaxPointers APS5_VABI __std_minmax_element_1_nid_postfix(const void* first, const void* last, bool isSigned) { return MinMaxBySign<std::uint8_t>(first, last, isSigned, true, true); }
MinMaxPointers APS5_VABI __std_minmax_element_2_nid_postfix(const void* first, const void* last, bool isSigned) { return MinMaxBySign<std::uint16_t>(first, last, isSigned, true, true); }
MinMaxPointers APS5_VABI __std_minmax_element_4_nid_postfix(const void* first, const void* last, bool isSigned) { return MinMaxBySign<std::uint32_t>(first, last, isSigned, true, true); }
MinMaxPointers APS5_VABI __std_minmax_element_8_nid_postfix(const void* first, const void* last, bool isSigned) { return MinMaxBySign<std::uint64_t>(first, last, isSigned, true, true); }

void APS5_VABI __std_reverse_trivially_swappable_1_nid_postfix(void* first, void* last) { Reverse<1>(first, last); }
void APS5_VABI __std_reverse_trivially_swappable_2_nid_postfix(void* first, void* last) { Reverse<2>(first, last); }
void APS5_VABI __std_reverse_trivially_swappable_4_nid_postfix(void* first, void* last) { Reverse<4>(first, last); }
void APS5_VABI __std_reverse_trivially_swappable_8_nid_postfix(void* first, void* last) { Reverse<8>(first, last); }
void APS5_VABI __std_reverse_copy_trivially_copyable_1_nid_postfix(const void* first, const void* last, void* destination) { ReverseCopy<1>(first, last, destination); }
void APS5_VABI __std_reverse_copy_trivially_copyable_2_nid_postfix(const void* first, const void* last, void* destination) { ReverseCopy<2>(first, last, destination); }
void APS5_VABI __std_reverse_copy_trivially_copyable_4_nid_postfix(const void* first, const void* last, void* destination) { ReverseCopy<4>(first, last, destination); }
void APS5_VABI __std_reverse_copy_trivially_copyable_8_nid_postfix(const void* first, const void* last, void* destination) { ReverseCopy<8>(first, last, destination); }

void APS5_VABI __std_swap_ranges_trivially_swappable_noalias_nid_postfix(void* first1, void* last1, void* first2) {
    auto* left = static_cast<unsigned char*>(first1);
    auto* right = static_cast<unsigned char*>(first2);
    const std::size_t length = static_cast<std::size_t>(static_cast<unsigned char*>(last1) - left);
    for (std::size_t i = 0; i < length; ++i) {
        const unsigned char byte = left[i];
        left[i] = right[i];
        right[i] = byte;
    }
}

}
