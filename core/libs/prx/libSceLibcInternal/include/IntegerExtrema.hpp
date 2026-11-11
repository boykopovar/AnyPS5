#pragma once

#include "SceTypes.hpp"

#include <cstdint>

template<typename TValue>
struct IntegerMinMax {
    TValue minimum;
    TValue maximum;
};

extern "C" {

std::int8_t APS5_VABI __std_min_1i_nid_postfix(const void* first, const void* last);
std::int8_t APS5_VABI __std_max_1i_nid_postfix(const void* first, const void* last);
std::int16_t APS5_VABI __std_min_2i_nid_postfix(const void* first, const void* last);
std::int16_t APS5_VABI __std_max_2i_nid_postfix(const void* first, const void* last);
std::uint16_t APS5_VABI __std_min_2u_nid_postfix(const void* first, const void* last);
std::uint16_t APS5_VABI __std_max_2u_nid_postfix(const void* first, const void* last);
IntegerMinMax<std::int8_t> APS5_VABI __std_minmax_1i_nid_postfix(const void* first, const void* last);
IntegerMinMax<std::uint8_t> APS5_VABI __std_minmax_1u_nid_postfix(const void* first, const void* last);
IntegerMinMax<std::int16_t> APS5_VABI __std_minmax_2i_nid_postfix(const void* first, const void* last);
IntegerMinMax<std::uint16_t> APS5_VABI __std_minmax_2u_nid_postfix(const void* first, const void* last);
IntegerMinMax<std::int32_t> APS5_VABI __std_minmax_4i_nid_postfix(const void* first, const void* last);
IntegerMinMax<std::uint32_t> APS5_VABI __std_minmax_4u_nid_postfix(const void* first, const void* last);
IntegerMinMax<std::int64_t> APS5_VABI __std_minmax_8i_nid_postfix(const void* first, const void* last);
IntegerMinMax<std::uint64_t> APS5_VABI __std_minmax_8u_nid_postfix(const void* first, const void* last);

}
