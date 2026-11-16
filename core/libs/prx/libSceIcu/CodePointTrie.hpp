#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UCPMap;
struct UCPTrie;
struct UMutableCPTrie;

namespace Icu {
using CodePointValueFilter = std::uint32_t (APS5_VABI*)(const void*, std::uint32_t);
} // namespace Icu

extern "C" {
std::uint32_t APS5_VABI ucpmap_get_nid_postfix(const UCPMap*, std::int32_t);
std::int32_t APS5_VABI ucpmap_getRange_nid_postfix(const UCPMap*, std::int32_t, std::int32_t, std::uint32_t, Icu::CodePointValueFilter, const void*, std::uint32_t*);
UCPTrie* APS5_VABI ucptrie_openFromBinary_nid_postfix(std::int32_t, std::int32_t, const void*, std::int32_t, std::int32_t*, std::int32_t*);
void APS5_VABI ucptrie_close_nid_postfix(UCPTrie*);
std::int32_t APS5_VABI ucptrie_getType_nid_postfix(const UCPTrie*);
std::int32_t APS5_VABI ucptrie_getValueWidth_nid_postfix(const UCPTrie*);
std::uint32_t APS5_VABI ucptrie_get_nid_postfix(const UCPTrie*, std::int32_t);
std::int32_t APS5_VABI ucptrie_getRange_nid_postfix(const UCPTrie*, std::int32_t, std::int32_t, std::uint32_t, Icu::CodePointValueFilter, const void*, std::uint32_t*);
std::int32_t APS5_VABI ucptrie_toBinary_nid_postfix(const UCPTrie*, void*, std::int32_t, std::int32_t*);
std::int32_t APS5_VABI ucptrie_internalSmallIndex_nid_postfix(const UCPTrie*, std::int32_t);
std::int32_t APS5_VABI ucptrie_internalSmallU8Index_nid_postfix(const UCPTrie*, std::int32_t, std::uint8_t, std::uint8_t);
std::int32_t APS5_VABI ucptrie_internalU8PrevIndex_nid_postfix(const UCPTrie*, std::int32_t, const std::uint8_t*, const std::uint8_t*);
UMutableCPTrie* APS5_VABI umutablecptrie_open_nid_postfix(std::uint32_t, std::uint32_t, std::int32_t*);
UMutableCPTrie* APS5_VABI umutablecptrie_clone_nid_postfix(const UMutableCPTrie*, std::int32_t*);
void APS5_VABI umutablecptrie_close_nid_postfix(UMutableCPTrie*);
UMutableCPTrie* APS5_VABI umutablecptrie_fromUCPMap_nid_postfix(const UCPMap*, std::int32_t*);
UMutableCPTrie* APS5_VABI umutablecptrie_fromUCPTrie_nid_postfix(const UCPTrie*, std::int32_t*);
std::uint32_t APS5_VABI umutablecptrie_get_nid_postfix(const UMutableCPTrie*, std::int32_t);
std::int32_t APS5_VABI umutablecptrie_getRange_nid_postfix(const UMutableCPTrie*, std::int32_t, std::int32_t, std::uint32_t, Icu::CodePointValueFilter, const void*, std::uint32_t*);
void APS5_VABI umutablecptrie_set_nid_postfix(UMutableCPTrie*, std::int32_t, std::uint32_t, std::int32_t*);
void APS5_VABI umutablecptrie_setRange_nid_postfix(UMutableCPTrie*, std::int32_t, std::int32_t, std::uint32_t, std::int32_t*);
UCPTrie* APS5_VABI umutablecptrie_buildImmutable_nid_postfix(UMutableCPTrie*, std::int32_t, std::int32_t, std::int32_t*);
}
