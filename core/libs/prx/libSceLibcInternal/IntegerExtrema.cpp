#include "prx/libSceLibcInternal/include/IntegerExtrema.hpp"

#include <cstdint>
#include <cstring>

#include "prx/libc/include/General.hpp"

namespace {

template<typename TValue>
TValue Read(const unsigned char* address) {
    TValue value;
    std::memcpy(&value, address, sizeof(value));
    return value;
}

template<typename TValue>
IntegerMinMax<TValue> Extrema(const void* first, const void* last, const char* name) {
    const auto beginAddress = reinterpret_cast<std::uintptr_t>(first);
    const auto endAddress = reinterpret_cast<std::uintptr_t>(last);
    if (!first || !last || endAddress <= beginAddress || (endAddress - beginAddress) % sizeof(TValue) != 0) NotImplemented_nid_no_patch(name);
    auto* current = static_cast<const unsigned char*>(first);
    const auto* end = static_cast<const unsigned char*>(last);
    const TValue initial = Read<TValue>(current);
    IntegerMinMax<TValue> result{initial, initial};
    for (current += sizeof(TValue); current != end; current += sizeof(TValue)) {
        const auto value = Read<TValue>(current);
        if (value < result.minimum) result.minimum = value;
        if (value > result.maximum) result.maximum = value;
    }
    return result;
}

} // namespace

extern "C" {

std::int8_t APS5_VABI __std_min_1i_nid_postfix(const void* first, const void* last) { return Extrema<std::int8_t>(first, last, __func__).minimum; }
std::int8_t APS5_VABI __std_max_1i_nid_postfix(const void* first, const void* last) { return Extrema<std::int8_t>(first, last, __func__).maximum; }
std::int16_t APS5_VABI __std_min_2i_nid_postfix(const void* first, const void* last) { return Extrema<std::int16_t>(first, last, __func__).minimum; }
std::int16_t APS5_VABI __std_max_2i_nid_postfix(const void* first, const void* last) { return Extrema<std::int16_t>(first, last, __func__).maximum; }
std::uint16_t APS5_VABI __std_min_2u_nid_postfix(const void* first, const void* last) { return Extrema<std::uint16_t>(first, last, __func__).minimum; }
std::uint16_t APS5_VABI __std_max_2u_nid_postfix(const void* first, const void* last) { return Extrema<std::uint16_t>(first, last, __func__).maximum; }
IntegerMinMax<std::int8_t> APS5_VABI __std_minmax_1i_nid_postfix(const void* first, const void* last) { return Extrema<std::int8_t>(first, last, __func__); }
IntegerMinMax<std::uint8_t> APS5_VABI __std_minmax_1u_nid_postfix(const void* first, const void* last) { return Extrema<std::uint8_t>(first, last, __func__); }
IntegerMinMax<std::int16_t> APS5_VABI __std_minmax_2i_nid_postfix(const void* first, const void* last) { return Extrema<std::int16_t>(first, last, __func__); }
IntegerMinMax<std::uint16_t> APS5_VABI __std_minmax_2u_nid_postfix(const void* first, const void* last) { return Extrema<std::uint16_t>(first, last, __func__); }
IntegerMinMax<std::int32_t> APS5_VABI __std_minmax_4i_nid_postfix(const void* first, const void* last) { return Extrema<std::int32_t>(first, last, __func__); }
IntegerMinMax<std::uint32_t> APS5_VABI __std_minmax_4u_nid_postfix(const void* first, const void* last) { return Extrema<std::uint32_t>(first, last, __func__); }
IntegerMinMax<std::int64_t> APS5_VABI __std_minmax_8i_nid_postfix(const void* first, const void* last) { return Extrema<std::int64_t>(first, last, __func__); }
IntegerMinMax<std::uint64_t> APS5_VABI __std_minmax_8u_nid_postfix(const void* first, const void* last) { return Extrema<std::uint64_t>(first, last, __func__); }

}
