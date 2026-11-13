#pragma once

#include "SceTypes.hpp"

extern "C" {

void APS5_VABI __std_bitset_to_string_1_nid_postfix(char* destination, const void* source, std::size_t bits, char zero, char one);
void APS5_VABI _ZNSt8__sce_v215_Thrd_sleep_forEm_nid_postfix(std::uint64_t milliseconds);
int APS5_VABI __std_atomic_notify_multi_direct_nid_postfix();
std::uint8_t APS5_VABI __std_max_1u_nid_postfix(const void* first, const void* last);
std::uint8_t APS5_VABI __std_min_1u_nid_postfix(const void* first, const void* last);
int APS5_VABI __truncdfhf2_nid_postfix();
int APS5_VABI sceLibcInternalPrimitiveMallocStatsFast_nid_postfix();
int APS5_VABI __std_atomic_wait_direct_8_nid_postfix();
int APS5_VABI sceLibcInternalPrimitiveMallocStats_nid_postfix();
int APS5_VABI __std_atomic_wait_direct_16_nid_postfix();
void APS5_VABI __std_bitset_to_string_2_nid_postfix(std::uint16_t* destination, const void* source, std::size_t bits, std::uint16_t zero, std::uint16_t one);

}
