// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#pragma once
#include "Object.hpp"
#include "ByteSink.hpp"
#include "CharString.hpp"
#include "Edits.hpp"

namespace Icu::Abi {
struct CharStringByteSink {
    const GuestFunction* vtable;
    CharString* destination;
};
static_assert(sizeof(CharStringByteSink) == 16 && alignof(CharStringByteSink) == 8);
static_assert(offsetof(CharStringByteSink, vtable) == 0 && offsetof(CharStringByteSink, destination) == 8);
}

extern "C" {
void APS5_VABI _ZN6icu_6818CharStringByteSinkC1EPNS_10CharStringE_nid_postfix(Icu::Abi::CharStringByteSink* self, Icu::Abi::CharString* destination);
void APS5_VABI _ZN6icu_6818CharStringByteSinkC2EPNS_10CharStringE_nid_postfix(Icu::Abi::CharStringByteSink* self, Icu::Abi::CharString* destination);
void APS5_VABI _ZN6icu_6818CharStringByteSinkD1Ev_nid_postfix(Icu::Abi::CharStringByteSink* self);
void APS5_VABI _ZN6icu_6818CharStringByteSinkD2Ev_nid_postfix(Icu::Abi::CharStringByteSink* self);
void APS5_VABI _ZN6icu_6818CharStringByteSinkD0Ev_nid_postfix(Icu::Abi::CharStringByteSink* self);
void APS5_VABI _ZN6icu_6818CharStringByteSink6AppendEPKci_nid_postfix(Icu::Abi::CharStringByteSink* self, const char* bytes, std::int32_t length);
char* APS5_VABI _ZN6icu_6818CharStringByteSink15GetAppendBufferEiiPciPi_nid_postfix(Icu::Abi::CharStringByteSink* self, std::int32_t minimum, std::int32_t desired, char* scratch, std::int32_t scratchCapacity, std::int32_t* capacity);
void APS5_VABI _ZN6icu_6812ByteSinkUtil14appendTwoBytesEiRNS_8ByteSinkE_nid_postfix(std::int32_t point, Icu::Abi::ByteSink* sink);
void APS5_VABI _ZN6icu_6812ByteSinkUtil15appendCodePointEiiRNS_8ByteSinkEPNS_5EditsE_nid_postfix(std::int32_t length, std::int32_t point, Icu::Abi::ByteSink* sink, Icu::Abi::Edits* edits);
void APS5_VABI _ZN6icu_6812ByteSinkUtil23appendNonEmptyUnchangedEPKhiRNS_8ByteSinkEjPNS_5EditsE_nid_postfix(const std::uint8_t* source, std::int32_t length, Icu::Abi::ByteSink* sink, std::uint32_t options, Icu::Abi::Edits* edits);
std::int8_t APS5_VABI _ZN6icu_6812ByteSinkUtil15appendUnchangedEPKhS2_RNS_8ByteSinkEjPNS_5EditsER10UErrorCode_nid_postfix(const std::uint8_t* source, const std::uint8_t* limit, Icu::Abi::ByteSink* sink, std::uint32_t options, Icu::Abi::Edits* edits, std::int32_t* status);
std::int8_t APS5_VABI _ZN6icu_6812ByteSinkUtil12appendChangeEiPKDsiRNS_8ByteSinkEPNS_5EditsER10UErrorCode_nid_postfix(std::int32_t length, const char16_t* source, std::int32_t sourceLength, Icu::Abi::ByteSink* sink, Icu::Abi::Edits* edits, std::int32_t* status);
std::int8_t APS5_VABI _ZN6icu_6812ByteSinkUtil12appendChangeEPKhS2_PKDsiRNS_8ByteSinkEPNS_5EditsER10UErrorCode_nid_postfix(const std::uint8_t* source, const std::uint8_t* limit, const char16_t* replacement, std::int32_t replacementLength, Icu::Abi::ByteSink* sink, Icu::Abi::Edits* edits, std::int32_t* status);
}
