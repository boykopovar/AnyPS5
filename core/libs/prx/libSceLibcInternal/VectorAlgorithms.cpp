#include "prx/libSceLibcInternal/include/VectorAlgorithms.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

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

float APS5_VABI __std_min_f_nid_postfix(const void* first, const void* last) { return ValueAt<float>(ScanNonEmpty<float>(first, last, true, false, __func__).minimum); }
double APS5_VABI __std_min_d_nid_postfix(const void* first, const void* last) { return ValueAt<double>(ScanNonEmpty<double>(first, last, true, false, __func__).minimum); }
float APS5_VABI __std_max_f_nid_postfix(const void* first, const void* last) { return ValueAt<float>(ScanNonEmpty<float>(first, last, false, true, __func__).maximum); }
double APS5_VABI __std_max_d_nid_postfix(const void* first, const void* last) { return ValueAt<double>(ScanNonEmpty<double>(first, last, false, true, __func__).maximum); }

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
