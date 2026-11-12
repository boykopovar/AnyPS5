#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

namespace Icu {
struct BytesTrie;
struct UCharsTrie;
struct ByteSink;
struct Appendable;
} // namespace Icu

extern "C" {
void APS5_VABI _ZN6icu_689BytesTrieD1Ev_nid_postfix(Icu::BytesTrie* self);
void APS5_VABI _ZN6icu_689BytesTrieD2Ev_nid_postfix(Icu::BytesTrie* self);
std::int32_t APS5_VABI _ZN6icu_689BytesTrie9readValueEPKhi_nid_postfix(const std::uint8_t* pos, std::int32_t leadByte);
const std::uint8_t* APS5_VABI _ZN6icu_689BytesTrie11jumpByDeltaEPKh_nid_postfix(const std::uint8_t* pos);
std::int32_t APS5_VABI _ZNK6icu_689BytesTrie7currentEv_nid_postfix(const Icu::BytesTrie* self);
std::int32_t APS5_VABI _ZN6icu_689BytesTrie10branchNextEPKhii_nid_postfix(Icu::BytesTrie* self, const std::uint8_t* pos,
                                                                         std::int32_t length, std::int32_t inByte);
std::int32_t APS5_VABI _ZN6icu_689BytesTrie8nextImplEPKhi_nid_postfix(Icu::BytesTrie* self, const std::uint8_t* pos,
                                                                     std::int32_t inByte);
std::int32_t APS5_VABI _ZN6icu_689BytesTrie4nextEi_nid_postfix(Icu::BytesTrie* self, std::int32_t inByte);
std::int32_t APS5_VABI _ZN6icu_689BytesTrie4nextEPKci_nid_postfix(Icu::BytesTrie* self, const char* text, std::int32_t length);
const std::uint8_t* APS5_VABI _ZN6icu_689BytesTrie25findUniqueValueFromBranchEPKhiaRi_nid_postfix(const std::uint8_t* pos,
                                                                                                std::int32_t length,
                                                                                                std::int8_t haveUniqueValue,
                                                                                                std::int32_t* uniqueValue);
std::int8_t APS5_VABI _ZN6icu_689BytesTrie15findUniqueValueEPKhaRi_nid_postfix(const std::uint8_t* pos, std::int8_t haveUniqueValue,
                                                                              std::int32_t* uniqueValue);
std::int32_t APS5_VABI _ZNK6icu_689BytesTrie12getNextBytesERNS_8ByteSinkE_nid_postfix(const Icu::BytesTrie* self, Icu::ByteSink* out);
void APS5_VABI _ZN6icu_689BytesTrie18getNextBranchBytesEPKhiRNS_8ByteSinkE_nid_postfix(const std::uint8_t* pos, std::int32_t length,
                                                                                      Icu::ByteSink* out);
void APS5_VABI _ZN6icu_689BytesTrie6appendERNS_8ByteSinkEi_nid_postfix(Icu::ByteSink* out, std::int32_t byte);

void APS5_VABI _ZN6icu_6810UCharsTrieD1Ev_nid_postfix(Icu::UCharsTrie* self);
void APS5_VABI _ZN6icu_6810UCharsTrieD2Ev_nid_postfix(Icu::UCharsTrie* self);
std::int32_t APS5_VABI _ZNK6icu_6810UCharsTrie7currentEv_nid_postfix(const Icu::UCharsTrie* self);
std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie17firstForCodePointEi_nid_postfix(Icu::UCharsTrie* self, std::int32_t codePoint);
std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie16nextForCodePointEi_nid_postfix(Icu::UCharsTrie* self, std::int32_t codePoint);
std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie10branchNextEPKDsii_nid_postfix(Icu::UCharsTrie* self, const char16_t* pos,
                                                                            std::int32_t length, std::int32_t unit);
std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie8nextImplEPKDsi_nid_postfix(Icu::UCharsTrie* self, const char16_t* pos, std::int32_t unit);
std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie4nextEi_nid_postfix(Icu::UCharsTrie* self, std::int32_t unit);
std::int32_t APS5_VABI _ZN6icu_6810UCharsTrie4nextENS_14ConstChar16PtrEi_nid_postfix(Icu::UCharsTrie* self, const char16_t* const* text,
                                                                                    std::int32_t length);
const char16_t* APS5_VABI _ZN6icu_6810UCharsTrie25findUniqueValueFromBranchEPKDsiaRi_nid_postfix(const char16_t* pos,
                                                                                               std::int32_t length,
                                                                                               std::int8_t haveUniqueValue,
                                                                                               std::int32_t* uniqueValue);
std::int8_t APS5_VABI _ZN6icu_6810UCharsTrie15findUniqueValueEPKDsaRi_nid_postfix(const char16_t* pos, std::int8_t haveUniqueValue,
                                                                                 std::int32_t* uniqueValue);
std::int32_t APS5_VABI _ZNK6icu_6810UCharsTrie13getNextUCharsERNS_10AppendableE_nid_postfix(const Icu::UCharsTrie* self,
                                                                                           Icu::Appendable* out);
void APS5_VABI _ZN6icu_6810UCharsTrie19getNextBranchUCharsEPKDsiRNS_10AppendableE_nid_postfix(const char16_t* pos, std::int32_t length,
                                                                                             Icu::Appendable* out);
}
