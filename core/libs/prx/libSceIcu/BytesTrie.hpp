// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#pragma once
#include "Vector.hpp"
#include "ByteSink.hpp"

namespace Icu::Abi {

struct BytesTrie {
    std::uint8_t* ownedArray;
    const std::uint8_t* bytes;
    const std::uint8_t* position;
    std::int32_t remaining;
};

struct ByteSinkFunctions {
    void (APS5_VABI *destroy)(ByteSink*);
    void (APS5_VABI *deleteObject)(ByteSink*);
    void (APS5_VABI *append)(ByteSink*, const char*, std::int32_t);
    char* (APS5_VABI *getAppendBuffer)(ByteSink*, std::int32_t, std::int32_t, char*, std::int32_t, std::int32_t*);
    void (APS5_VABI *flush)(ByteSink*);
};

static_assert(sizeof(BytesTrie) == 32 && alignof(BytesTrie) == 8);
static_assert(offsetof(BytesTrie, ownedArray) == 0 && offsetof(BytesTrie, bytes) == 8);
static_assert(offsetof(BytesTrie, position) == 16 && offsetof(BytesTrie, remaining) == 24);
static_assert(sizeof(ByteSinkFunctions) == 40 && alignof(ByteSinkFunctions) == 8);
static_assert(offsetof(ByteSinkFunctions, append) == 16 && offsetof(ByteSinkFunctions, flush) == 32);

}
extern "C" {
void APS5_VABI _ZN6icu_689BytesTrieD1Ev_nid_postfix(Icu::Abi::BytesTrie* self);
void APS5_VABI _ZN6icu_689BytesTrieD2Ev_nid_postfix(Icu::Abi::BytesTrie* self);
std::int32_t APS5_VABI _ZN6icu_689BytesTrie9readValueEPKhi_nid_postfix(const std::uint8_t* position, std::int32_t lead);
const std::uint8_t* APS5_VABI _ZN6icu_689BytesTrie11jumpByDeltaEPKh_nid_postfix(const std::uint8_t* position);
std::int32_t APS5_VABI _ZNK6icu_689BytesTrie7currentEv_nid_postfix(const Icu::Abi::BytesTrie* self);
std::int32_t APS5_VABI _ZN6icu_689BytesTrie10branchNextEPKhii_nid_postfix(Icu::Abi::BytesTrie* self, const std::uint8_t* position, std::int32_t length, std::int32_t input);
std::int32_t APS5_VABI _ZN6icu_689BytesTrie8nextImplEPKhi_nid_postfix(Icu::Abi::BytesTrie* self, const std::uint8_t* position, std::int32_t input);
std::int32_t APS5_VABI _ZN6icu_689BytesTrie4nextEi_nid_postfix(Icu::Abi::BytesTrie* self, std::int32_t input);
std::int32_t APS5_VABI _ZN6icu_689BytesTrie4nextEPKci_nid_postfix(Icu::Abi::BytesTrie* self, const char* text, std::int32_t length);
std::int8_t APS5_VABI _ZN6icu_689BytesTrie15findUniqueValueEPKhaRi_nid_postfix(const std::uint8_t* position, std::int8_t haveValue, std::int32_t* value);
const std::uint8_t* APS5_VABI _ZN6icu_689BytesTrie25findUniqueValueFromBranchEPKhiaRi_nid_postfix(const std::uint8_t* position, std::int32_t length, std::int8_t haveValue, std::int32_t* value);
void APS5_VABI _ZN6icu_689BytesTrie6appendERNS_8ByteSinkEi_nid_postfix(Icu::Abi::ByteSink* output, std::int32_t value);
void APS5_VABI _ZN6icu_689BytesTrie18getNextBranchBytesEPKhiRNS_8ByteSinkE_nid_postfix(const std::uint8_t* position, std::int32_t length, Icu::Abi::ByteSink* output);
std::int32_t APS5_VABI _ZNK6icu_689BytesTrie12getNextBytesERNS_8ByteSinkE_nid_postfix(const Icu::Abi::BytesTrie* self, Icu::Abi::ByteSink* output);
}
