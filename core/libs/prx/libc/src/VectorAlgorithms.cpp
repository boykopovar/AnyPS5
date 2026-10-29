#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <bit>
#include <type_traits>

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
#include <emmintrin.h>
#define APS5_LIBC_HAS_SSE2 1
#endif

#include "prx/libc/include/General.hpp"

namespace {

template<typename TValue>
TValue Read(const unsigned char* address) {
    TValue value;
    std::memcpy(&value, address, sizeof(value));
    return value;
}

#if defined(APS5_LIBC_HAS_SSE2)
template<typename TValue>
__m128i Broadcast(TValue value) {
    if constexpr (sizeof(TValue) == 1) {
        return _mm_set1_epi8(static_cast<char>(value));
    } else if constexpr (sizeof(TValue) == 2) {
        return _mm_set1_epi16(static_cast<short>(value));
    } else if constexpr (sizeof(TValue) == 4) {
        return _mm_set1_epi32(static_cast<int>(value));
    } else {
        const auto low = static_cast<std::uint32_t>(value);
        const auto high = static_cast<std::uint32_t>(value >> 32);
        return _mm_set_epi32(static_cast<int>(high), static_cast<int>(low),
                             static_cast<int>(high), static_cast<int>(low));
    }
}

template<typename TValue>
__m128i CompareEqual(__m128i left, __m128i right) {
    if constexpr (sizeof(TValue) == 1) return _mm_cmpeq_epi8(left, right);
    if constexpr (sizeof(TValue) == 2) return _mm_cmpeq_epi16(left, right);
    return _mm_cmpeq_epi32(left, right);
}

template<typename TValue>
std::uint32_t MatchPositions(__m128i equal) {
    const auto bits = static_cast<std::uint32_t>(_mm_movemask_epi8(equal));
    if constexpr (sizeof(TValue) == 1) return bits;
    if constexpr (sizeof(TValue) == 2) return bits & (bits >> 1) & 0x5555u;
    if constexpr (sizeof(TValue) == 4) return bits & (bits >> 1) & (bits >> 2) & (bits >> 3) & 0x1111u;
    return ((bits & 0x00ffu) == 0x00ffu ? 1u : 0u) |
           ((bits & 0xff00u) == 0xff00u ? 2u : 0u);
}

template<typename TValue>
__m128i CountMatchBytes(__m128i equal) {
    if constexpr (sizeof(TValue) == 8) {
        const auto adjacent = _mm_shuffle_epi32(equal, _MM_SHUFFLE(2, 3, 0, 1));
        equal = _mm_and_si128(equal, adjacent);
    }
    return _mm_sad_epu8(equal, _mm_setzero_si128());
}
#endif

template<typename TValue>
const void* Find(const void* first, const void* last, TValue value) {
    auto* current = static_cast<const unsigned char*>(first);
    auto* end = static_cast<const unsigned char*>(last);
#if defined(APS5_LIBC_HAS_SSE2)
    if constexpr (sizeof(TValue) <= 4) {
        const auto target = Broadcast(value);
        while (static_cast<std::size_t>(end - current) >= 16) {
            const auto data = _mm_loadu_si128(reinterpret_cast<const __m128i*>(current));
            const auto positions = MatchPositions<TValue>(CompareEqual<TValue>(data, target));
            if (positions) {
                const auto lane = std::countr_zero(positions) / sizeof(TValue);
                return current + lane * sizeof(TValue);
            }
            current += 16;
        }
    }
#endif
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
#if defined(APS5_LIBC_HAS_SSE2)
    if constexpr (sizeof(TValue) <= 2) {
        auto* blockEnd = block + UnsizedBlock;
        if (auto* found = static_cast<const unsigned char*>(Find(start, blockEnd, value)); found != blockEnd) return found;
        auto* current = block;
        for (;;) {
            auto* next = current + 16;
            if (auto* found = static_cast<const unsigned char*>(Find(current, next, value)); found != next) return found;
            current = next;
        }
    }
#endif
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
#if defined(APS5_LIBC_HAS_SSE2)
    if constexpr (sizeof(TValue) <= 4) {
        const auto target = Broadcast(value);
        auto byteCount = _mm_setzero_si128();
        while (static_cast<std::size_t>(end - current) >= 16) {
            const auto data = _mm_loadu_si128(reinterpret_cast<const __m128i*>(current));
            const auto equal = CompareEqual<TValue>(data, target);
            byteCount = _mm_add_epi64(byteCount, CountMatchBytes<TValue>(equal));
            current += 16;
        }
        const auto totalBytes = static_cast<std::uint64_t>(_mm_cvtsi128_si64(byteCount)) +
                                static_cast<std::uint64_t>(_mm_cvtsi128_si64(_mm_srli_si128(byteCount, 8)));
        count += static_cast<std::size_t>(totalBytes / (255 * sizeof(TValue)));
    }
#endif
    for (; current != end; current += sizeof(TValue)) {
        if (Read<TValue>(current) == value) ++count;
    }
    return count;
}

template<typename TValue>
MinMaxPointers MinMax(const void* first, const void* last, bool wantMinimum, bool wantMaximum);

#if defined(APS5_LIBC_HAS_SSE2)
template<typename TValue>
const void* MinElementSse2(const void* first, const void* last) {
    auto* begin = static_cast<const unsigned char*>(first);
    auto* current = begin;
    auto* end = static_cast<const unsigned char*>(last);
    if (static_cast<std::size_t>(end - begin) < 64) return MinMax<TValue>(first, last, true, false).minimum;

    auto minimum = _mm_loadu_si128(reinterpret_cast<const __m128i*>(current));
    current += 16;
    while (static_cast<std::size_t>(end - current) >= 16) {
        const auto values = _mm_loadu_si128(reinterpret_cast<const __m128i*>(current));
        if constexpr (sizeof(TValue) == 1) {
            if constexpr (std::is_signed_v<TValue>) {
                const auto takeValues = _mm_cmpgt_epi8(minimum, values);
                minimum = _mm_or_si128(_mm_and_si128(takeValues, values), _mm_andnot_si128(takeValues, minimum));
            } else {
                minimum = _mm_min_epu8(minimum, values);
            }
        } else {
            if constexpr (std::is_signed_v<TValue>) {
                minimum = _mm_min_epi16(minimum, values);
            } else {
                const auto sign = _mm_set1_epi16(static_cast<short>(0x8000));
                const auto left = _mm_xor_si128(minimum, sign);
                const auto right = _mm_xor_si128(values, sign);
                const auto takeValues = _mm_cmpgt_epi16(left, right);
                minimum = _mm_or_si128(_mm_and_si128(takeValues, values), _mm_andnot_si128(takeValues, minimum));
            }
        }
        current += 16;
    }

    alignas(16) TValue lanes[16 / sizeof(TValue)];
    _mm_store_si128(reinterpret_cast<__m128i*>(lanes), minimum);
    TValue minValue = lanes[0];
    for (const auto value : lanes) if (value < minValue) minValue = value;
    for (; current != end; current += sizeof(TValue)) {
        const auto value = Read<TValue>(current);
        if (value < minValue) minValue = value;
    }
    for (current = begin; current != end; current += sizeof(TValue)) {
        if (Read<TValue>(current) == minValue) return current;
    }
    return first;
}

template<typename TUnsigned>
const void* MinElementBySignSse2(const void* first, const void* last, bool isSigned) {
    using TSigned = std::make_signed_t<TUnsigned>;
    return isSigned ? MinElementSse2<TSigned>(first, last) : MinElementSse2<TUnsigned>(first, last);
}
#endif

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

std::int64_t APS5_VABI __std_max_8i_nid_postfix(const void* first, const void* last) {
    const auto beginAddress = reinterpret_cast<std::uintptr_t>(first);
    const auto endAddress = reinterpret_cast<std::uintptr_t>(last);
    if (!first || !last || endAddress <= beginAddress || (endAddress - beginAddress) % sizeof(std::int64_t) != 0) NotImplemented_nid_no_patch(__func__);
    auto* current = static_cast<const unsigned char*>(first);
    const auto* end = static_cast<const unsigned char*>(last);
    std::int64_t maximum = Read<std::int64_t>(current);
    for (current += sizeof(std::int64_t); current != end; current += sizeof(std::int64_t)) {
        const auto value = Read<std::int64_t>(current);
        if (value > maximum) maximum = value;
    }
    return maximum;
}

std::size_t APS5_VABI __std_count_trivial_1_nid_postfix(const void* first, const void* last, std::uint8_t value) { return Count(first, last, value); }
std::size_t APS5_VABI __std_count_trivial_2_nid_postfix(const void* first, const void* last, std::uint16_t value) { return Count(first, last, value); }
std::size_t APS5_VABI __std_count_trivial_4_nid_postfix(const void* first, const void* last, std::uint32_t value) { return Count(first, last, value); }
std::size_t APS5_VABI __std_count_trivial_8_nid_postfix(const void* first, const void* last, std::uint64_t value) { return Count(first, last, value); }

std::uint64_t APS5_VABI __std_min_8u_nid_postfix(const void* first, const void* last) {
    const auto beginAddress = reinterpret_cast<std::uintptr_t>(first);
    const auto endAddress = reinterpret_cast<std::uintptr_t>(last);
    if (!first || !last || endAddress <= beginAddress || (endAddress - beginAddress) % sizeof(std::uint64_t) != 0) NotImplemented_nid_no_patch(__func__);
    auto* current = static_cast<const unsigned char*>(first);
    const auto* end = static_cast<const unsigned char*>(last);
    std::uint64_t minimum = Read<std::uint64_t>(current);
    for (current += sizeof(std::uint64_t); current != end; current += sizeof(std::uint64_t)) {
        const auto value = Read<std::uint64_t>(current);
        if (value < minimum) minimum = value;
    }
    return minimum;
}

const void* APS5_VABI __std_min_element_1_nid_postfix(const void* first, const void* last, bool isSigned) {
#if defined(APS5_LIBC_HAS_SSE2)
    return MinElementBySignSse2<std::uint8_t>(first, last, isSigned);
#else
    return MinMaxBySign<std::uint8_t>(first, last, isSigned, true, false).minimum;
#endif
}
const void* APS5_VABI __std_min_element_2_nid_postfix(const void* first, const void* last, bool isSigned) {
#if defined(APS5_LIBC_HAS_SSE2)
    return MinElementBySignSse2<std::uint16_t>(first, last, isSigned);
#else
    return MinMaxBySign<std::uint16_t>(first, last, isSigned, true, false).minimum;
#endif
}
const void* APS5_VABI __std_min_element_4_nid_postfix(const void* first, const void* last, bool isSigned) { return MinMaxBySign<std::uint32_t>(first, last, isSigned, true, false).minimum; }
const void* APS5_VABI __std_min_element_8_nid_postfix(const void* first, const void* last, bool isSigned) { return MinMaxBySign<std::uint64_t>(first, last, isSigned, true, false).minimum; }

std::int32_t APS5_VABI __std_min_4i_nid_postfix(const void* first, const void* last) {
    const auto beginAddress = reinterpret_cast<std::uintptr_t>(first);
    const auto endAddress = reinterpret_cast<std::uintptr_t>(last);
    if (!first || !last || endAddress <= beginAddress || (endAddress - beginAddress) % sizeof(std::int32_t) != 0) NotImplemented_nid_no_patch(__func__);
    auto* current = static_cast<const unsigned char*>(first);
    const auto* end = static_cast<const unsigned char*>(last);
    std::int32_t minimum = Read<std::int32_t>(current);
    for (current += sizeof(std::int32_t); current != end; current += sizeof(std::int32_t)) {
        const auto value = Read<std::int32_t>(current);
        if (value < minimum) minimum = value;
    }
    return minimum;
}

const void* APS5_VABI __std_max_element_1_nid_postfix(const void* first, const void* last, bool isSigned) { return MinMaxBySign<std::uint8_t>(first, last, isSigned, false, true).maximum; }
const void* APS5_VABI __std_max_element_2_nid_postfix(const void* first, const void* last, bool isSigned) { return MinMaxBySign<std::uint16_t>(first, last, isSigned, false, true).maximum; }

std::int32_t APS5_VABI __std_max_4i_nid_postfix(const void* first, const void* last) {
    const auto beginAddress = reinterpret_cast<std::uintptr_t>(first);
    const auto endAddress = reinterpret_cast<std::uintptr_t>(last);
    if (!first || !last || endAddress <= beginAddress || (endAddress - beginAddress) % sizeof(std::int32_t) != 0) NotImplemented_nid_no_patch(__func__);
    auto* current = static_cast<const unsigned char*>(first);
    const auto* end = static_cast<const unsigned char*>(last);
    std::int32_t maximum = Read<std::int32_t>(current);
    for (current += sizeof(std::int32_t); current != end; current += sizeof(std::int32_t)) {
        const auto value = Read<std::int32_t>(current);
        if (value > maximum) maximum = value;
    }
    return maximum;
}

const void* APS5_VABI __std_max_element_4_nid_postfix(const void* first, const void* last, bool isSigned) { return MinMaxBySign<std::uint32_t>(first, last, isSigned, false, true).maximum; }
const void* APS5_VABI __std_max_element_8_nid_postfix(const void* first, const void* last, bool isSigned) { return MinMaxBySign<std::uint64_t>(first, last, isSigned, false, true).maximum; }
std::uint64_t APS5_VABI __std_max_8u_nid_postfix(const void* first, const void* last) {
    const auto beginAddress = reinterpret_cast<std::uintptr_t>(first);
    const auto endAddress = reinterpret_cast<std::uintptr_t>(last);
    if (!first || !last || endAddress <= beginAddress || (endAddress - beginAddress) % sizeof(std::uint64_t) != 0) NotImplemented_nid_no_patch(__func__);
    auto* current = static_cast<const unsigned char*>(first);
    const auto* end = static_cast<const unsigned char*>(last);
    std::uint64_t maximum = Read<std::uint64_t>(current);
    for (current += sizeof(std::uint64_t); current != end; current += sizeof(std::uint64_t)) {
        const auto value = Read<std::uint64_t>(current);
        if (maximum < value) maximum = value;
    }
    return maximum;
}

MinMaxPointers APS5_VABI __std_minmax_element_1_nid_postfix(const void* first, const void* last, bool isSigned) { return MinMaxBySign<std::uint8_t>(first, last, isSigned, true, true); }
MinMaxPointers APS5_VABI __std_minmax_element_2_nid_postfix(const void* first, const void* last, bool isSigned) { return MinMaxBySign<std::uint16_t>(first, last, isSigned, true, true); }
MinMaxPointers APS5_VABI __std_minmax_element_4_nid_postfix(const void* first, const void* last, bool isSigned) { return MinMaxBySign<std::uint32_t>(first, last, isSigned, true, true); }
MinMaxPointers APS5_VABI __std_minmax_element_8_nid_postfix(const void* first, const void* last, bool isSigned) { return MinMaxBySign<std::uint64_t>(first, last, isSigned, true, true); }

std::uint32_t APS5_VABI __std_min_4u_nid_postfix(const void* first, const void* last) {
    const auto beginAddress = reinterpret_cast<std::uintptr_t>(first);
    const auto endAddress = reinterpret_cast<std::uintptr_t>(last);
    if (!first || !last || endAddress <= beginAddress || (endAddress - beginAddress) % sizeof(std::uint32_t) != 0) NotImplemented_nid_no_patch(__func__);
    auto* current = static_cast<const unsigned char*>(first);
    const auto* end = static_cast<const unsigned char*>(last);
    std::uint32_t minimum = Read<std::uint32_t>(current);
    for (current += sizeof(std::uint32_t); current != end; current += sizeof(std::uint32_t)) {
        const auto value = Read<std::uint32_t>(current);
        if (value < minimum) minimum = value;
    }
    return minimum;
}

void APS5_VABI __std_reverse_trivially_swappable_1_nid_postfix(void* first, void* last) { Reverse<1>(first, last); }
void APS5_VABI __std_reverse_trivially_swappable_2_nid_postfix(void* first, void* last) { Reverse<2>(first, last); }
void APS5_VABI __std_reverse_trivially_swappable_4_nid_postfix(void* first, void* last) { Reverse<4>(first, last); }
void APS5_VABI __std_reverse_trivially_swappable_8_nid_postfix(void* first, void* last) { Reverse<8>(first, last); }
std::uint32_t APS5_VABI __std_max_4u_nid_postfix(const void* first, const void* last) {
    const auto beginAddress = reinterpret_cast<std::uintptr_t>(first);
    const auto endAddress = reinterpret_cast<std::uintptr_t>(last);
    if (!first || !last || endAddress <= beginAddress || (endAddress - beginAddress) % sizeof(std::uint32_t) != 0) NotImplemented_nid_no_patch(__func__);
    auto* current = static_cast<const unsigned char*>(first);
    const auto* end = static_cast<const unsigned char*>(last);
    std::uint32_t maximum = Read<std::uint32_t>(current);
    for (current += sizeof(std::uint32_t); current != end; current += sizeof(std::uint32_t)) {
        const auto value = Read<std::uint32_t>(current);
        if (value > maximum) maximum = value;
    }
    return maximum;
}

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

std::int64_t APS5_VABI __std_min_8i_nid_postfix(const void* first, const void* last) {
    const auto beginAddress = reinterpret_cast<std::uintptr_t>(first);
    const auto endAddress = reinterpret_cast<std::uintptr_t>(last);
    if (!first || !last || endAddress <= beginAddress || (endAddress - beginAddress) % sizeof(std::int64_t) != 0) NotImplemented_nid_no_patch(__func__);
    auto* current = static_cast<const unsigned char*>(first);
    const auto* end = static_cast<const unsigned char*>(last);
    std::int64_t minimum = Read<std::int64_t>(current);
    for (current += sizeof(std::int64_t); current != end; current += sizeof(std::int64_t)) {
        const auto value = Read<std::int64_t>(current);
        if (value < minimum) minimum = value;
    }
    return minimum;
}

}
