// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include "StringPiece.hpp"
#include "UnicodeString.hpp"

namespace Icu::Abi {

struct CharString {
    struct Buffer {
        char* pointer;
        std::int32_t capacity;
        std::int8_t needToRelease;
        char stack[40];
    } buffer;
    std::int32_t length;
};

static_assert(sizeof(CharString::Buffer) == 56);
static_assert(alignof(CharString::Buffer) == 8);
static_assert(offsetof(CharString::Buffer, pointer) == 0);
static_assert(offsetof(CharString::Buffer, capacity) == 8);
static_assert(offsetof(CharString::Buffer, needToRelease) == 12);
static_assert(offsetof(CharString::Buffer, stack) == 13);
static_assert(sizeof(CharString) == 64);
static_assert(alignof(CharString) == 8);
static_assert(offsetof(CharString, buffer) == 0);
static_assert(offsetof(CharString, length) == 56);

}

extern "C" {
Icu::Abi::CharString* APS5_VABI _ZN6icu_6810CharString8truncateEi_nid_postfix(Icu::Abi::CharString*, std::int32_t);
std::int32_t APS5_VABI _ZNK6icu_6810CharString11lastIndexOfEc_nid_postfix(const Icu::Abi::CharString*, char);
char APS5_VABI _ZNK6icu_6810CharString13getDirSepCharEv_nid_postfix(const Icu::Abi::CharString*);
std::int32_t APS5_VABI _ZNK6icu_6810CharString7extractEPciR10UErrorCode_nid_postfix(const Icu::Abi::CharString*, char*, std::int32_t, std::int32_t*);
std::int8_t APS5_VABI _ZN6icu_6810CharString14ensureCapacityEiiR10UErrorCode_nid_postfix(Icu::Abi::CharString* text, std::int32_t capacity, std::int32_t desired, std::int32_t* status);
Icu::Abi::CharString* APS5_VABI _ZN6icu_6810CharString6appendEcR10UErrorCode_nid_postfix(Icu::Abi::CharString* text, char unit, std::int32_t* status);
Icu::Abi::CharString* APS5_VABI _ZN6icu_6810CharString6appendEPKciR10UErrorCode_nid_postfix(Icu::Abi::CharString* text, const char* source, std::int32_t count, std::int32_t* status);
Icu::Abi::CharString* APS5_VABI _ZN6icu_6810CharString8copyFromERKS0_R10UErrorCode_nid_postfix(Icu::Abi::CharString* text, const Icu::Abi::CharString* source, std::int32_t* status);
void APS5_VABI _ZN6icu_6810CharStringC1EOS0__nid_postfix(Icu::Abi::CharString* text, Icu::Abi::CharString* source);
void APS5_VABI _ZN6icu_6810CharStringC2EOS0__nid_postfix(Icu::Abi::CharString* text, Icu::Abi::CharString* source);
Icu::Abi::CharString* APS5_VABI _ZN6icu_6810CharStringaSEOS0__nid_postfix(Icu::Abi::CharString* text, Icu::Abi::CharString* source);
bool APS5_VABI _ZNK6icu_6810CharString8containsENS_11StringPieceE_nid_postfix(const Icu::Abi::CharString* text, Icu::Abi::StringPiece piece);
char* APS5_VABI _ZN6icu_6810CharString15getAppendBufferEiiRiR10UErrorCode_nid_postfix(Icu::Abi::CharString* text, std::int32_t minimum, std::int32_t desired, std::int32_t* result, std::int32_t* status);
Icu::Abi::CharString* APS5_VABI _ZN6icu_6810CharString20appendInvariantCharsEPKDsiR10UErrorCode_nid_postfix(Icu::Abi::CharString* text, const char16_t* source, std::int32_t length, std::int32_t* status);
Icu::Abi::CharString* APS5_VABI _ZN6icu_6810CharString20appendInvariantCharsERKNS_13UnicodeStringER10UErrorCode_nid_postfix(Icu::Abi::CharString* text, const Icu::Abi::UnicodeString* source, std::int32_t* status);
Icu::Abi::CharString* APS5_VABI _ZN6icu_6810CharString27ensureEndsWithFileSeparatorER10UErrorCode_nid_postfix(Icu::Abi::CharString* text, std::int32_t* status);
Icu::Abi::CharString* APS5_VABI _ZN6icu_6810CharString14appendPathPartENS_11StringPieceER10UErrorCode_nid_postfix(Icu::Abi::CharString* text, Icu::Abi::StringPiece piece, std::int32_t* status);
}
