#pragma once

#include "general/VabiMacros.hpp"
#include <cstdint>

extern "C" {

std::uint64_t APS5_VABI _ZNKSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE4findEcm_nid_postfix(
    const void* self, char value, std::uint64_t position);
std::uint64_t APS5_VABI _ZNKSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE5rfindEcm_nid_postfix(
    const void* self, char value, std::uint64_t position);

}
