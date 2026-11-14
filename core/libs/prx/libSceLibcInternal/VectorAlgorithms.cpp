#include "prx/libSceLibcInternal/include/VectorAlgorithms.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
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
void RejectNaN(const void* first, const void* last, const char* name) {
    auto* current = static_cast<const unsigned char*>(first);
    auto* end = static_cast<const unsigned char*>(last);
    for (; current != end; current += sizeof(TValue)) {
        const TValue value = Read<TValue>(current);
        if (value != value) NotImplemented_nid_no_patch(name);
    }
}

template<typename TValue>
MinMaxPointers Scan(const void* first, const void* last, bool wantMinimum, bool wantMaximum, const char* name, bool lastLargest = false) {
    MinMaxPointers result{first, first};
    if (first == last) return result;
    if (!first || !last || !IsValidRange<TValue>(first, last)) NotImplemented_nid_no_patch(name);
    RejectNaN<TValue>(first, last, name);
    auto* current = static_cast<const unsigned char*>(first);
    auto* end = static_cast<const unsigned char*>(last);
    TValue minimum = Read<TValue>(current);
    TValue maximum = minimum;
    for (current += sizeof(TValue); current != end; current += sizeof(TValue)) {
        const TValue value = Read<TValue>(current);
        if (wantMinimum && value < minimum) {
            minimum = value;
            result.minimum = current;
        }
        if (wantMaximum && (lastLargest ? !(value < maximum) : maximum < value)) {
            maximum = value;
            result.maximum = current;
        }
    }
    return result;
}

template<typename TValue>
MinMaxPointers ScanNonEmpty(const void* first, const void* last, bool wantMinimum, bool wantMaximum, const char* name) {
    if (first == last) NotImplemented_nid_no_patch(name);
    return Scan<TValue>(first, last, wantMinimum, wantMaximum, name);
}

template<typename TValue>
TValue ValueAt(const void* address) {
    return Read<TValue>(static_cast<const unsigned char*>(address));
}

}

extern "C" {

float APS5_VABI __std_min_f_nid_postfix(const void* first, const void* last) { return ValueExtremum<float, true>(first, last, __func__); }
double APS5_VABI __std_min_d_nid_postfix(const void* first, const void* last) { return ValueExtremum<double, true>(first, last, __func__); }
float APS5_VABI __std_max_f_nid_postfix(const void* first, const void* last) { return ValueExtremum<float, false>(first, last, __func__); }
double APS5_VABI __std_max_d_nid_postfix(const void* first, const void* last) { return ValueExtremum<double, false>(first, last, __func__); }

MinMaxFloat APS5_VABI __std_minmax_f_nid_postfix(const void* first, const void* last) {
    const MinMaxPointers found = ScanNonEmpty<float>(first, last, true, true, __func__);
    return {ValueAt<float>(found.minimum), ValueAt<float>(found.maximum)};
}

MinMaxDouble APS5_VABI __std_minmax_d_nid_postfix(const void* first, const void* last) {
    const MinMaxPointers found = ScanNonEmpty<double>(first, last, true, true, __func__);
    return {ValueAt<double>(found.minimum), ValueAt<double>(found.maximum)};
}

const void* APS5_VABI __std_min_element_f_nid_postfix(const void* first, const void* last, bool) { return Scan<float>(first, last, true, false, __func__).minimum; }
const void* APS5_VABI __std_min_element_d_nid_postfix(const void* first, const void* last, bool) { return Scan<double>(first, last, true, false, __func__).minimum; }
const void* APS5_VABI __std_max_element_f_nid_postfix(const void* first, const void* last, bool) { return Scan<float>(first, last, false, true, __func__).maximum; }
const void* APS5_VABI __std_max_element_d_nid_postfix(const void* first, const void* last, bool) { return Scan<double>(first, last, false, true, __func__).maximum; }
MinMaxPointers APS5_VABI __std_minmax_element_f_nid_postfix(const void* first, const void* last, bool) { return Scan<float>(first, last, true, true, __func__, true); }
MinMaxPointers APS5_VABI __std_minmax_element_d_nid_postfix(const void* first, const void* last, bool) { return Scan<double>(first, last, true, true, __func__, true); }

}
