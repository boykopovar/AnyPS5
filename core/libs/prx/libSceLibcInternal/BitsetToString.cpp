#include "prx/libSceLibcInternal/include/RecoveredInventory.hpp"
#include "prx/libc/include/General.hpp"

namespace {

template <typename TElement>
void BitsetToString(TElement* destination, const void* source, std::size_t bits, TElement zero, TElement one) {
    if (bits == 0) return;
    if (!destination || !source) APS5_INVALID_ARG_EX;
    const auto* bytes = static_cast<const std::uint8_t*>(source);
    for (std::size_t bit = 0; bit < bits; ++bit) destination[bits - 1 - bit] = ((bytes[bit >> 3] >> (bit & 7)) & 1u) != 0 ? one : zero;
}

}

extern "C" {

void APS5_VABI __std_bitset_to_string_1_nid_postfix(char* destination, const void* source, std::size_t bits, char zero, char one) {
    BitsetToString(destination, source, bits, zero, one);
}

void APS5_VABI __std_bitset_to_string_2_nid_postfix(std::uint16_t* destination, const void* source, std::size_t bits, std::uint16_t zero, std::uint16_t one) {
    BitsetToString(destination, source, bits, zero, one);
}

}
