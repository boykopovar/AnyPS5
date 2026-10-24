#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

struct MinMaxPointers {
    const void* minimum;
    const void* maximum;
};

extern "C" {
const void* APS5_VABI __std_find_trivial_2_nid_postfix(const void*, const void*, std::uint16_t);
const void* APS5_VABI __std_find_trivial_unsized_1_nid_postfix(const void*, std::uint8_t);
const void* APS5_VABI __std_find_trivial_unsized_4_nid_postfix(const void*, std::uint32_t);
std::size_t APS5_VABI __std_count_trivial_8_nid_postfix(const void*, const void*, std::uint64_t);
const void* APS5_VABI __std_min_element_1_nid_postfix(const void*, const void*, bool);
const void* APS5_VABI __std_max_element_4_nid_postfix(const void*, const void*, bool);
MinMaxPointers APS5_VABI __std_minmax_element_2_nid_postfix(const void*, const void*, bool);
MinMaxPointers APS5_VABI __std_minmax_element_8_nid_postfix(const void*, const void*, bool);
void APS5_VABI __std_reverse_trivially_swappable_2_nid_postfix(void*, void*);
void APS5_VABI __std_reverse_copy_trivially_copyable_8_nid_postfix(const void*, const void*, void*);
void APS5_VABI __std_swap_ranges_trivially_swappable_noalias_nid_postfix(void*, void*, void*);
}

static void Require(bool value) { if (!value) std::abort(); }

static void CheckFindAndCount() {
    const std::uint16_t halves[] = {5, 7, 9, 7};
    Require(__std_find_trivial_2_nid_postfix(halves, halves + 4, 7) == halves + 1);
    Require(__std_find_trivial_2_nid_postfix(halves, halves + 4, 8) == halves + 4);
    Require(__std_find_trivial_2_nid_postfix(halves, halves, 5) == halves);
    const std::uint64_t wides[] = {1, 2, 1, 1};
    Require(__std_count_trivial_8_nid_postfix(wides, wides + 4, 1) == 3 && __std_count_trivial_8_nid_postfix(wides, wides, 1) == 0);
    alignas(32) std::uint32_t words[16] = {};
    words[3] = 42;
    words[12] = 42;
    Require(__std_find_trivial_unsized_4_nid_postfix(words + 8, 42) == words + 12);
}

static void CheckUnsizedRescan() {
    alignas(32) unsigned char bytes[96] = {};
    bytes[3] = 'x';
    bytes[70] = 'x';
    Require(__std_find_trivial_unsized_1_nid_postfix(bytes + 8, 'x') == bytes + 3);
    bytes[20] = 'x';
    Require(__std_find_trivial_unsized_1_nid_postfix(bytes + 8, 'x') == bytes + 20);
    bytes[3] = 0;
    bytes[20] = 0;
    Require(__std_find_trivial_unsized_1_nid_postfix(bytes + 8, 'x') == bytes + 70);
}

static void CheckMinMax() {
    const unsigned char bytes[] = {3, 0x80, 1, 0x80, 1, 0xff};
    Require(__std_min_element_1_nid_postfix(bytes, bytes + 6, false) == bytes + 2);
    Require(__std_min_element_1_nid_postfix(bytes, bytes + 6, true) == bytes + 1);
    Require(__std_min_element_1_nid_postfix(bytes, bytes, true) == bytes);
    const std::uint32_t words[] = {7, 0xffffffffu, 9, 0xffffffffu, 9};
    Require(__std_max_element_4_nid_postfix(words, words + 5, false) == words + 1);
    Require(__std_max_element_4_nid_postfix(words, words + 5, true) == words + 2);
    const std::int16_t halves[] = {4, -3, 8, -3, 8, 2};
    const MinMaxPointers signedHalves = __std_minmax_element_2_nid_postfix(halves, halves + 6, true);
    Require(signedHalves.minimum == halves + 1 && signedHalves.maximum == halves + 4);
    const MinMaxPointers unsignedHalves = __std_minmax_element_2_nid_postfix(halves, halves + 6, false);
    Require(unsignedHalves.minimum == halves + 5 && unsignedHalves.maximum == halves + 3);
    const std::uint64_t wides[] = {5};
    const MinMaxPointers single = __std_minmax_element_8_nid_postfix(wides, wides + 1, false);
    Require(single.minimum == wides && single.maximum == wides);
    const MinMaxPointers empty = __std_minmax_element_8_nid_postfix(wides, wides, false);
    Require(empty.minimum == wides && empty.maximum == wides);
}

static void CheckReverseAndSwap() {
    std::uint16_t halves[] = {1, 2, 3, 4, 5};
    __std_reverse_trivially_swappable_2_nid_postfix(halves, halves + 5);
    Require(halves[0] == 5 && halves[1] == 4 && halves[2] == 3 && halves[3] == 2 && halves[4] == 1);
    const std::uint64_t wides[] = {1, 2, 3};
    std::uint64_t reversed[4] = {9, 9, 9, 9};
    __std_reverse_copy_trivially_copyable_8_nid_postfix(wides, wides + 3, reversed);
    Require(reversed[0] == 3 && reversed[1] == 2 && reversed[2] == 1 && reversed[3] == 9);
    unsigned char left[37], right[37];
    for (int i = 0; i < 37; ++i) {
        left[i] = static_cast<unsigned char>(i);
        right[i] = static_cast<unsigned char>(100 + i);
    }
    __std_swap_ranges_trivially_swappable_noalias_nid_postfix(left, left + 37, right);
    for (int i = 0; i < 37; ++i) Require(left[i] == 100 + i && right[i] == i);
}

int main() {
    CheckFindAndCount();
    CheckUnsizedRescan();
    CheckMinMax();
    CheckReverseAndSwap();
    return 0;
}
