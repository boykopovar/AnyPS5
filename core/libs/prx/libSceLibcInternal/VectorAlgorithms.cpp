#include "prx/libSceLibcInternal/include/VectorAlgorithms.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <emmintrin.h>

namespace {

template<typename TValue>
TValue Read(const unsigned char* address) {
    TValue value;
    std::memcpy(&value, address, sizeof(value));
    return value;
}

template<typename TValue>
bool IsValidRange(const void* first, const void* last) {
    const auto beginAddress = reinterpret_cast<std::uintptr_t>(first);
    const auto endAddress = reinterpret_cast<std::uintptr_t>(last);
    return endAddress >= beginAddress && (endAddress - beginAddress) % sizeof(TValue) == 0;
}

template<bool TMinimum>
__m128 PackedExtremum(__m128 first, __m128 second) {
    if constexpr (TMinimum) return _mm_min_ps(first, second);
    else return _mm_max_ps(first, second);
}

template<bool TMinimum>
__m128d PackedExtremum(__m128d first, __m128d second) {
    if constexpr (TMinimum) return _mm_min_pd(first, second);
    else return _mm_max_pd(first, second);
}

template<bool TMinimum>
__m128 ScalarExtremum(__m128 first, __m128 second) {
    if constexpr (TMinimum) return _mm_min_ss(first, second);
    else return _mm_max_ss(first, second);
}

template<bool TMinimum>
__m128d ScalarExtremum(__m128d first, __m128d second) {
    if constexpr (TMinimum) return _mm_min_sd(first, second);
    else return _mm_max_sd(first, second);
}

template<typename TValue, bool TMinimum>
TValue ValueExtremum(const void* first, const void* last, const char* name) {
    if (!first || !last || first == last || !IsValidRange<TValue>(first, last)) NotImplemented_nid_no_patch(name);
    const auto scalar = [](const unsigned char* address) {
        if constexpr (sizeof(TValue) == 4) return _mm_set_ss(Read<float>(address));
        else return _mm_set_sd(Read<double>(address));
    };
    const auto packed = [](const unsigned char* address) {
        if constexpr (sizeof(TValue) == 4) return _mm_loadu_ps(reinterpret_cast<const float*>(address));
        else return _mm_loadu_pd(reinterpret_cast<const double*>(address));
    };
    auto* current = static_cast<const unsigned char*>(first);
    const auto* end = static_cast<const unsigned char*>(last);
    auto result = scalar(current);
    if (end - current >= 16) {
        result = packed(current);
        current += 16;
        while (end - current >= 16) {
            result = PackedExtremum<TMinimum>(result, packed(current));
            current += 16;
        }
        if constexpr (sizeof(TValue) == 4) {
            result = PackedExtremum<TMinimum>(result, _mm_shuffle_ps(result, result, _MM_SHUFFLE(1, 0, 3, 2)));
            result = ScalarExtremum<TMinimum>(result, _mm_shuffle_ps(result, result, _MM_SHUFFLE(3, 3, 1, 1)));
        } else {
            result = ScalarExtremum<TMinimum>(result, _mm_unpackhi_pd(result, result));
        }
    } else {
        current += sizeof(TValue);
    }
    for (; current != end; current += sizeof(TValue)) result = ScalarExtremum<TMinimum>(scalar(current), result);
    if constexpr (sizeof(TValue) == 4) return _mm_cvtss_f32(result);
    else return _mm_cvtsd_f64(result);
}

template<typename TValue>
auto LoadScalar(const unsigned char* address) {
    if constexpr (sizeof(TValue) == 4) return _mm_load_ss(reinterpret_cast<const float*>(address));
    else return _mm_load_sd(reinterpret_cast<const double*>(address));
}

template<typename TValue>
auto LoadPacked(const unsigned char* address) {
    if constexpr (sizeof(TValue) == 4) return _mm_loadu_ps(reinterpret_cast<const float*>(address));
    else return _mm_loadu_pd(reinterpret_cast<const double*>(address));
}

__m128 Scalar(float value) { return _mm_set_ss(value); }
__m128d Scalar(double value) { return _mm_set_sd(value); }
float ScalarValue(__m128 value) { return _mm_cvtss_f32(value); }
double ScalarValue(__m128d value) { return _mm_cvtsd_f64(value); }
int LessMask(__m128 first, __m128 second) { return _mm_movemask_ps(_mm_cmplt_ps(first, second)); }
int LessMask(__m128d first, __m128d second) { return _mm_movemask_pd(_mm_cmplt_pd(first, second)); }
int EqualMask(__m128 first, __m128 second) { return _mm_movemask_ps(_mm_cmpeq_ps(first, second)); }
int EqualMask(__m128d first, __m128d second) { return _mm_movemask_pd(_mm_cmpeq_pd(first, second)); }
bool Greater(__m128 first, __m128 second) { return _mm_ucomigt_ss(first, second); }
bool Greater(__m128d first, __m128d second) { return _mm_ucomigt_sd(first, second); }
bool GreaterEqual(__m128 first, __m128 second) { return _mm_ucomige_ss(first, second); }
bool GreaterEqual(__m128d first, __m128d second) { return _mm_ucomige_sd(first, second); }

__m128 LastMaximum(__m128 value, __m128 current) {
    const auto keep = _mm_cmpnle_ss(current, value);
    return _mm_or_ps(_mm_and_ps(keep, current), _mm_andnot_ps(keep, value));
}

__m128d LastMaximum(__m128d value, __m128d current) {
    const auto keep = _mm_cmpnle_sd(current, value);
    return _mm_or_pd(_mm_and_pd(keep, current), _mm_andnot_pd(keep, value));
}

template<bool TMinimum>
__m128 ReduceLanes(__m128 value) {
    value = PackedExtremum<TMinimum>(value, _mm_shuffle_ps(value, value, _MM_SHUFFLE(1, 0, 3, 2)));
    return PackedExtremum<TMinimum>(value, _mm_shuffle_ps(value, value, _MM_SHUFFLE(2, 3, 0, 1)));
}

template<bool TMinimum>
__m128d ReduceLanes(__m128d value) {
    return PackedExtremum<TMinimum>(value, _mm_shuffle_pd(value, value, 1));
}

template<bool TMinimum>
__m128 ReduceValue(__m128 value) {
    value = PackedExtremum<TMinimum>(value, _mm_shuffle_ps(value, value, _MM_SHUFFLE(1, 0, 3, 2)));
    return ScalarExtremum<TMinimum>(value, _mm_shuffle_ps(value, value, _MM_SHUFFLE(3, 3, 1, 1)));
}

template<bool TMinimum>
__m128d ReduceValue(__m128d value) {
    return ScalarExtremum<TMinimum>(value, _mm_unpackhi_pd(value, value));
}

template<typename TValue>
auto PairExtremum(const void* first, const void* last, const char* name) {
    if (!first || !last || first == last || !IsValidRange<TValue>(first, last)) NotImplemented_nid_no_patch(name);
    auto* current = static_cast<const unsigned char*>(first);
    const auto* end = static_cast<const unsigned char*>(last);
    auto minimum = LoadScalar<TValue>(current);
    auto maximum = minimum;
    if (end - current >= 16) {
        minimum = LoadPacked<TValue>(current);
        maximum = minimum;
        current += 16;
        while (end - current >= 16) {
            const auto next = LoadPacked<TValue>(current);
            minimum = PackedExtremum<true>(minimum, next);
            maximum = PackedExtremum<false>(maximum, next);
            current += 16;
        }
        minimum = ReduceValue<true>(minimum);
        maximum = ReduceValue<false>(maximum);
    } else current += sizeof(TValue);
    for (; current != end; current += sizeof(TValue)) {
        const auto next = LoadScalar<TValue>(current);
        minimum = ScalarExtremum<true>(next, minimum);
        maximum = ScalarExtremum<false>(next, maximum);
    }
    if constexpr (sizeof(TValue) == 4) return MinMaxFloat{ScalarValue(minimum), ScalarValue(maximum)};
    else return MinMaxDouble{ScalarValue(minimum), ScalarValue(maximum)};
}

template<typename TValue, bool TLast>
const unsigned char* SelectElement(const unsigned char* first, const std::array<std::uint64_t, 4>& indices, int mask, const char* name) {
    constexpr std::size_t lanes = 16 / sizeof(TValue);
    std::size_t selected = lanes;
    for (std::size_t lane = 0; lane < lanes; ++lane) {
        if (!(mask & (1 << lane))) continue;
        if (selected == lanes || (TLast ? indices[lane] >= indices[selected] : indices[lane] < indices[selected])) selected = lane;
    }
    if (selected == lanes) {
        if constexpr (sizeof(TValue) == 4 || TLast) NotImplemented_nid_no_patch(name);
        else selected = 0;
    }
    return first + indices[selected] * 16 + selected * sizeof(TValue);
}

template<typename TValue, bool TMinimum, bool TMaximum, bool TLastMaximum = false>
MinMaxPointers ElementExtremum(const void* first, const void* last, const char* name) {
    MinMaxPointers result{first, first};
    if (first == last) return result;
    if (!first || !last || !IsValidRange<TValue>(first, last)) NotImplemented_nid_no_patch(name);
    auto* current = static_cast<const unsigned char*>(first);
    const auto* end = static_cast<const unsigned char*>(last);
    auto minimum = Scalar(std::numeric_limits<TValue>::infinity());
    auto maximum = Scalar(-std::numeric_limits<TValue>::infinity());
    const auto* vectorEnd = current + ((end - current) & ~std::ptrdiff_t{15});
    while (current != vectorEnd) {
        const auto remaining = static_cast<std::uint64_t>(vectorEnd - current);
        const auto chunkBytes = sizeof(TValue) == 4 && remaining > (std::uint64_t{1} << 36) ? (std::uint64_t{1} << 36) : remaining;
        const auto* chunk = current;
        const auto* chunkEnd = current + chunkBytes;
        auto minValues = LoadPacked<TValue>(current);
        auto maxValues = minValues;
        std::array<std::uint64_t, 4> minIndices{};
        std::array<std::uint64_t, 4> maxIndices{};
        current += 16;
        for (std::uint64_t block = 1; current != chunkEnd; ++block, current += 16) {
            const auto next = LoadPacked<TValue>(current);
            int minMask = 0;
            int maxMask = 0;
            if constexpr (TMinimum) minMask = LessMask(next, minValues);
            if constexpr (TMaximum) maxMask = TLastMaximum ? LessMask(next, maxValues) : LessMask(maxValues, next);
            if constexpr (TMinimum) minValues = PackedExtremum<true>(minValues, next);
            if constexpr (TMaximum) maxValues = TLastMaximum ? PackedExtremum<false>(next, maxValues) : PackedExtremum<false>(maxValues, next);
            for (std::size_t lane = 0; lane < 16 / sizeof(TValue); ++lane) {
                if constexpr (TMinimum) if (minMask & (1 << lane)) minIndices[lane] = block;
                if constexpr (TMaximum) if (static_cast<bool>(maxMask & (1 << lane)) != TLastMaximum) maxIndices[lane] = block;
            }
        }
        if constexpr (TMinimum) {
            const auto lower = ReduceLanes<true>(minValues);
            if (Greater(minimum, lower)) {
                result.minimum = SelectElement<TValue, false>(chunk, minIndices, EqualMask(lower, minValues), name);
                minimum = lower;
            }
        }
        if constexpr (TMaximum) {
            const auto upper = ReduceLanes<false>(maxValues);
            if (TLastMaximum ? GreaterEqual(upper, maximum) : Greater(upper, maximum)) {
                result.maximum = SelectElement<TValue, TLastMaximum>(chunk, maxIndices, EqualMask(upper, maxValues), name);
                maximum = upper;
            }
        }
    }
    for (; current != end; current += sizeof(TValue)) {
        const auto next = LoadScalar<TValue>(current);
        if constexpr (TMinimum) {
            if (Greater(minimum, next)) result.minimum = current;
            minimum = ScalarExtremum<true>(next, minimum);
        }
        if constexpr (TMaximum) {
            if (TLastMaximum ? GreaterEqual(next, maximum) : Greater(next, maximum)) result.maximum = current;
            if constexpr (TLastMaximum) maximum = LastMaximum(next, maximum);
            else maximum = ScalarExtremum<false>(next, maximum);
        }
    }
    return result;
}

}

extern "C" {

float APS5_VABI __std_min_f_nid_postfix(const void* first, const void* last) { return ValueExtremum<float, true>(first, last, __func__); }
double APS5_VABI __std_min_d_nid_postfix(const void* first, const void* last) { return ValueExtremum<double, true>(first, last, __func__); }
float APS5_VABI __std_max_f_nid_postfix(const void* first, const void* last) { return ValueExtremum<float, false>(first, last, __func__); }
double APS5_VABI __std_max_d_nid_postfix(const void* first, const void* last) { return ValueExtremum<double, false>(first, last, __func__); }

MinMaxFloat APS5_VABI __std_minmax_f_nid_postfix(const void* first, const void* last) {
    return PairExtremum<float>(first, last, __func__);
}

MinMaxDouble APS5_VABI __std_minmax_d_nid_postfix(const void* first, const void* last) {
    return PairExtremum<double>(first, last, __func__);
}

const void* APS5_VABI __std_min_element_f_nid_postfix(const void* first, const void* last, bool) { return ElementExtremum<float, true, false>(first, last, __func__).minimum; }
const void* APS5_VABI __std_min_element_d_nid_postfix(const void* first, const void* last, bool) { return ElementExtremum<double, true, false>(first, last, __func__).minimum; }
const void* APS5_VABI __std_max_element_f_nid_postfix(const void* first, const void* last, bool) { return ElementExtremum<float, false, true>(first, last, __func__).maximum; }
const void* APS5_VABI __std_max_element_d_nid_postfix(const void* first, const void* last, bool) { return ElementExtremum<double, false, true>(first, last, __func__).maximum; }
MinMaxPointers APS5_VABI __std_minmax_element_f_nid_postfix(const void* first, const void* last, bool) { return ElementExtremum<float, true, true, true>(first, last, __func__); }
MinMaxPointers APS5_VABI __std_minmax_element_d_nid_postfix(const void* first, const void* last, bool) { return ElementExtremum<double, true, true, true>(first, last, __func__); }

}
