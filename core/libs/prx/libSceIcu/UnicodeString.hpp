// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include "StringPiece.hpp"

namespace Icu::Abi {

struct UnicodeString {
    static constexpr std::int16_t IsBogus = 1;
    static constexpr std::int16_t UsingStackBuffer = 2;
    static constexpr std::int16_t RefCounted = 4;
    static constexpr std::int16_t BufferIsReadonly = 8;
    static constexpr std::int16_t OpenGetBuffer = 16;
    static constexpr std::int16_t WritableAlias = 0;
    static constexpr int LengthShift = 5;

    std::int16_t Flags() const;
    std::int32_t Length() const;
    std::int32_t Capacity() const;
    char16_t* Array() const;
    const char16_t* Buffer() const;
    bool Writable() const;
    bool BufferWritable() const;
    void Init();
    void Empty();
    void SetLength(std::int32_t length);
    void Release();
    void Bogus();
    bool Allocate(std::int32_t capacity);
    bool CloneBuffer(std::int32_t capacity = -1, std::int32_t grow = -1, bool copy = true, std::int32_t** deferred = nullptr, bool force = false);
    void Pin(std::int32_t& start, std::int32_t& count) const;
    UnicodeString* Append(const char16_t* source, std::int32_t start, std::int32_t count);
    UnicodeString* Replace(std::int32_t start, std::int32_t count, const char16_t* source, std::int32_t sourceStart, std::int32_t sourceLength);
    UnicodeString* CopyFrom(const UnicodeString* source, bool fast);
    void CopyFields(UnicodeString* source, bool reset);
    const void* vtable;
    union Storage {
        struct StackFields {
            std::int16_t lengthAndFlags;
            char16_t buffer[27];
        } stack;
        struct Fields {
            std::int16_t lengthAndFlags;
            std::int32_t length;
            std::int32_t capacity;
            char16_t* array;
        } fields;
    } storage;
};

static_assert(sizeof(UnicodeString) == 64);
static_assert(alignof(UnicodeString) == 8);
static_assert(offsetof(UnicodeString, vtable) == 0);
static_assert(offsetof(UnicodeString, storage) == 8);
static_assert(sizeof(UnicodeString::Storage) == 56);
static_assert(alignof(UnicodeString::Storage) == 8);
static_assert(sizeof(UnicodeString::Storage::StackFields) == 56);
static_assert(alignof(UnicodeString::Storage::StackFields) == 2);
static_assert(sizeof(UnicodeString::Storage::Fields) == 24);
static_assert(alignof(UnicodeString::Storage::Fields) == 8);
static_assert(offsetof(UnicodeString, storage.stack.lengthAndFlags) == 8);
static_assert(offsetof(UnicodeString, storage.stack.buffer) == 10);
static_assert(offsetof(UnicodeString, storage.fields.lengthAndFlags) == 8);
static_assert(offsetof(UnicodeString, storage.fields.length) == 12);
static_assert(offsetof(UnicodeString, storage.fields.capacity) == 16);
static_assert(offsetof(UnicodeString, storage.fields.array) == 24);

}

namespace Icu::Abi {
struct ConstChar16Ptr { const char16_t* pointer; };
struct Char16Ptr { char16_t* pointer; };
static_assert(sizeof(ConstChar16Ptr) == 8 && alignof(ConstChar16Ptr) == 8);
static_assert(sizeof(Char16Ptr) == 8 && alignof(Char16Ptr) == 8);
static_assert(offsetof(ConstChar16Ptr, pointer) == 0 && offsetof(Char16Ptr, pointer) == 0);
std::int32_t Utf16Length_nid_no_patch(const char16_t* text);
std::int32_t CheckedInt_nid_no_patch(std::int64_t value);
}

extern "C" {
std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString9getLengthEv_nid_postfix(const Icu::Abi::UnicodeString*);
char16_t APS5_VABI _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(const Icu::Abi::UnicodeString*, std::int32_t);
std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString11getChar32AtEi_nid_postfix(const Icu::Abi::UnicodeString*, std::int32_t);
std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString8char32AtEi_nid_postfix(const Icu::Abi::UnicodeString*, std::int32_t);
std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString14getChar32StartEi_nid_postfix(const Icu::Abi::UnicodeString*, std::int32_t);
std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString14getChar32LimitEi_nid_postfix(const Icu::Abi::UnicodeString*, std::int32_t);
std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString11countChar32Eii_nid_postfix(const Icu::Abi::UnicodeString*, std::int32_t, std::int32_t);
std::int8_t APS5_VABI _ZNK6icu_6813UnicodeString17hasMoreChar32ThanEiii_nid_postfix(const Icu::Abi::UnicodeString*, std::int32_t, std::int32_t, std::int32_t);
std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString11moveIndex32Eii_nid_postfix(const Icu::Abi::UnicodeString*, std::int32_t, std::int32_t);
std::int8_t APS5_VABI _ZNK6icu_6813UnicodeString8doEqualsERKS0_i_nid_postfix(const Icu::Abi::UnicodeString*, const Icu::Abi::UnicodeString*, std::int32_t);
std::int8_t APS5_VABI _ZNK6icu_6813UnicodeStringeqERKS0__nid_postfix(const Icu::Abi::UnicodeString*, const Icu::Abi::UnicodeString*);
std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString10doHashCodeEv_nid_postfix(const Icu::Abi::UnicodeString*);
std::int8_t APS5_VABI _ZNK6icu_6813UnicodeString11hasMetaDataEv_nid_postfix(const Icu::Abi::UnicodeString*);
void APS5_VABI _ZN6icu_6813UnicodeString7unBogusEv_nid_postfix(Icu::Abi::UnicodeString*);
std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString10unescapeAtERi_nid_postfix(const Icu::Abi::UnicodeString*, std::int32_t*);
void APS5_VABI _ZNK6icu_6813UnicodeString9doExtractEiiPDsi_nid_postfix(const Icu::Abi::UnicodeString*, std::int32_t, std::int32_t, char16_t*, std::int32_t);
void APS5_VABI _ZN6icu_6813UnicodeString6addRefEv_nid_postfix(Icu::Abi::UnicodeString*);
std::int32_t APS5_VABI _ZN6icu_6813UnicodeString9removeRefEv_nid_postfix(Icu::Abi::UnicodeString*);
std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString8refCountEv_nid_postfix(const Icu::Abi::UnicodeString*);
void APS5_VABI _ZN6icu_6813UnicodeString13releaseBufferEi_nid_postfix(Icu::Abi::UnicodeString*, std::int32_t);
std::int8_t APS5_VABI _ZN6icu_6813UnicodeString8allocateEi_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t capacity);
void APS5_VABI _ZN6icu_6813UnicodeString12releaseArrayEv_nid_postfix(Icu::Abi::UnicodeString* text);
void APS5_VABI _ZN6icu_6813UnicodeString10setToBogusEv_nid_postfix(Icu::Abi::UnicodeString* text);
std::int8_t APS5_VABI _ZN6icu_6813UnicodeString18cloneArrayIfNeededEiiaPPia_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t capacity, std::int32_t grow, std::int8_t copy, std::int32_t** deferred, std::int8_t force);
Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString8copyFromERKS0_a_nid_postfix(Icu::Abi::UnicodeString* text, const Icu::Abi::UnicodeString* source, std::int8_t fast);
Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeStringaSERKS0__nid_postfix(Icu::Abi::UnicodeString* text, const Icu::Abi::UnicodeString* source);
Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString12fastCopyFromERKS0__nid_postfix(Icu::Abi::UnicodeString* text, const Icu::Abi::UnicodeString* source);
void APS5_VABI _ZN6icu_6813UnicodeString14copyFieldsFromERS0_a_nid_postfix(Icu::Abi::UnicodeString* text, Icu::Abi::UnicodeString* source, std::int8_t reset);
Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeStringaSEOS0__nid_postfix(Icu::Abi::UnicodeString* text, Icu::Abi::UnicodeString* source);
void APS5_VABI _ZN6icu_6813UnicodeString4swapERS0__nid_postfix(Icu::Abi::UnicodeString* text, Icu::Abi::UnicodeString* other);
void APS5_VABI _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(Icu::Abi::UnicodeString* text);
void APS5_VABI _ZN6icu_6813UnicodeStringD2Ev_nid_postfix(Icu::Abi::UnicodeString* text);
void APS5_VABI _ZN6icu_6813UnicodeStringD0Ev_nid_postfix(Icu::Abi::UnicodeString* text);
Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString8doAppendEPKDsii_nid_postfix(Icu::Abi::UnicodeString* text, const char16_t* source, std::int32_t start, std::int32_t count);
Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString8doAppendERKS0_ii_nid_postfix(Icu::Abi::UnicodeString* text, const Icu::Abi::UnicodeString* source, std::int32_t start, std::int32_t count);
Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString9doReplaceEiiPKDsii_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t start, std::int32_t count, const char16_t* source, std::int32_t sourceStart, std::int32_t sourceLength);
Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString9doReplaceEiiRKS0_ii_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t start, std::int32_t count, const Icu::Abi::UnicodeString* source, std::int32_t sourceStart, std::int32_t sourceLength);
void APS5_VABI _ZN6icu_6813UnicodeStringC1EPKDsi_nid_postfix(Icu::Abi::UnicodeString* text, const char16_t* source, std::int32_t length);
void APS5_VABI _ZN6icu_6813UnicodeStringC1EPKDs_nid_postfix(Icu::Abi::UnicodeString* text, const char16_t* source);
void APS5_VABI _ZN6icu_6813UnicodeStringC1EDs_nid_postfix(Icu::Abi::UnicodeString* text, char16_t unit);
void APS5_VABI _ZN6icu_6813UnicodeStringC1Ei_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t point);
void APS5_VABI _ZN6icu_6813UnicodeStringC1Eiii_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t capacity, std::int32_t point, std::int32_t count);
void APS5_VABI _ZN6icu_6813UnicodeStringC1ERKS0__nid_postfix(Icu::Abi::UnicodeString* text, const Icu::Abi::UnicodeString* source);
void APS5_VABI _ZN6icu_6813UnicodeStringC1EOS0__nid_postfix(Icu::Abi::UnicodeString* text, Icu::Abi::UnicodeString* source);
void APS5_VABI _ZN6icu_6813UnicodeStringC1ERKS0_i_nid_postfix(Icu::Abi::UnicodeString* text, const Icu::Abi::UnicodeString* source, std::int32_t start);
void APS5_VABI _ZN6icu_6813UnicodeStringC1ERKS0_ii_nid_postfix(Icu::Abi::UnicodeString* text, const Icu::Abi::UnicodeString* source, std::int32_t start, std::int32_t count);
void APS5_VABI _ZN6icu_6813UnicodeStringC1EPDsii_nid_postfix(Icu::Abi::UnicodeString* text, char16_t* buffer, std::int32_t length, std::int32_t capacity);
void APS5_VABI _ZN6icu_6813UnicodeStringC1EaNS_14ConstChar16PtrEi_nid_postfix(Icu::Abi::UnicodeString* text, std::int8_t terminated, const Icu::Abi::ConstChar16Ptr* source, std::int32_t length);
void APS5_VABI _ZN6icu_6813UnicodeStringC1EPKciNS0_10EInvariantE_nid_postfix(Icu::Abi::UnicodeString* text, const char* source, std::int32_t length, std::int32_t);
void APS5_VABI _ZN6icu_6813UnicodeStringC2EPKDsi_nid_postfix(Icu::Abi::UnicodeString* text, const char16_t* source, std::int32_t length);
void APS5_VABI _ZN6icu_6813UnicodeStringC2EPKDs_nid_postfix(Icu::Abi::UnicodeString* text, const char16_t* source);
void APS5_VABI _ZN6icu_6813UnicodeStringC2EDs_nid_postfix(Icu::Abi::UnicodeString* text, char16_t unit);
void APS5_VABI _ZN6icu_6813UnicodeStringC2Ei_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t point);
void APS5_VABI _ZN6icu_6813UnicodeStringC2Eiii_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t capacity, std::int32_t point, std::int32_t count);
void APS5_VABI _ZN6icu_6813UnicodeStringC2ERKS0__nid_postfix(Icu::Abi::UnicodeString* text, const Icu::Abi::UnicodeString* source);
void APS5_VABI _ZN6icu_6813UnicodeStringC2EOS0__nid_postfix(Icu::Abi::UnicodeString* text, Icu::Abi::UnicodeString* source);
void APS5_VABI _ZN6icu_6813UnicodeStringC2ERKS0_i_nid_postfix(Icu::Abi::UnicodeString* text, const Icu::Abi::UnicodeString* source, std::int32_t start);
void APS5_VABI _ZN6icu_6813UnicodeStringC2ERKS0_ii_nid_postfix(Icu::Abi::UnicodeString* text, const Icu::Abi::UnicodeString* source, std::int32_t start, std::int32_t count);
void APS5_VABI _ZN6icu_6813UnicodeStringC2EPDsii_nid_postfix(Icu::Abi::UnicodeString* text, char16_t* buffer, std::int32_t length, std::int32_t capacity);
void APS5_VABI _ZN6icu_6813UnicodeStringC2EaNS_14ConstChar16PtrEi_nid_postfix(Icu::Abi::UnicodeString* text, std::int8_t terminated, const Icu::Abi::ConstChar16Ptr* source, std::int32_t length);
void APS5_VABI _ZN6icu_6813UnicodeStringC2EPKciNS0_10EInvariantE_nid_postfix(Icu::Abi::UnicodeString* text, const char* source, std::int32_t length, std::int32_t);
Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString5setToEPDsii_nid_postfix(Icu::Abi::UnicodeString* text, char16_t* buffer, std::int32_t length, std::int32_t capacity);
Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString5setToEaNS_14ConstChar16PtrEi_nid_postfix(Icu::Abi::UnicodeString* text, std::int8_t terminated, const Icu::Abi::ConstChar16Ptr* input, std::int32_t length);
char16_t* APS5_VABI _ZN6icu_6813UnicodeString9getBufferEi_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t capacity);
const char16_t* APS5_VABI _ZN6icu_6813UnicodeString19getTerminatedBufferEv_nid_postfix(Icu::Abi::UnicodeString* text);
void* APS5_VABI _ZN6icu_6813UnicodeString16getStaticClassIDEv_nid_postfix();
void* APS5_VABI _ZNK6icu_6813UnicodeString17getDynamicClassIDEv_nid_postfix(const Icu::Abi::UnicodeString*);
Icu::Abi::UnicodeString* APS5_VABI _ZNK6icu_6813UnicodeString5cloneEv_nid_postfix(const Icu::Abi::UnicodeString* text);
Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString5setToEDs_nid_postfix(Icu::Abi::UnicodeString* text, char16_t unit);
Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString9setCharAtEiDs_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t index, char16_t unit);
Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString6appendEi_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t point);
Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString7replaceEiii_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t start, std::int32_t count, std::int32_t point);
std::int8_t APS5_VABI _ZN6icu_6813UnicodeString10padLeadingEiDs_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t target, char16_t unit);
std::int8_t APS5_VABI _ZN6icu_6813UnicodeString11padTrailingEiDs_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t target, char16_t unit);
Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString9doReverseEii_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t start, std::int32_t count);
std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString7indexOfEPKDsiiii_nid_postfix(const Icu::Abi::UnicodeString* text, const char16_t* source, std::int32_t sourceStart, std::int32_t sourceLength, std::int32_t start, std::int32_t count);
std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString11lastIndexOfEPKDsiiii_nid_postfix(const Icu::Abi::UnicodeString* text, const char16_t* source, std::int32_t sourceStart, std::int32_t sourceLength, std::int32_t start, std::int32_t count);
std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString9doIndexOfEDsii_nid_postfix(const Icu::Abi::UnicodeString* text, char16_t point, std::int32_t start, std::int32_t count);
std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString9doIndexOfEiii_nid_postfix(const Icu::Abi::UnicodeString* text, std::int32_t point, std::int32_t start, std::int32_t count);
std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString13doLastIndexOfEDsii_nid_postfix(const Icu::Abi::UnicodeString* text, char16_t point, std::int32_t start, std::int32_t count);
std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString13doLastIndexOfEiii_nid_postfix(const Icu::Abi::UnicodeString* text, std::int32_t point, std::int32_t start, std::int32_t count);
std::int8_t APS5_VABI _ZNK6icu_6813UnicodeString9doCompareEiiPKDsii_nid_postfix(const Icu::Abi::UnicodeString* text, std::int32_t start, std::int32_t count, const char16_t* source, std::int32_t sourceStart, std::int32_t sourceLength);
std::int8_t APS5_VABI _ZNK6icu_6813UnicodeString23doCompareCodePointOrderEiiPKDsii_nid_postfix(const Icu::Abi::UnicodeString* text, std::int32_t start, std::int32_t count, const char16_t* source, std::int32_t sourceStart, std::int32_t sourceLength);
std::int8_t APS5_VABI _ZNK6icu_6813UnicodeString7compareENS_14ConstChar16PtrEi_nid_postfix(const Icu::Abi::UnicodeString* text, const Icu::Abi::ConstChar16Ptr* source, std::int32_t length);
std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString7extractENS_9Char16PtrEiR10UErrorCode_nid_postfix(const Icu::Abi::UnicodeString* text, const Icu::Abi::Char16Ptr* target, std::int32_t capacity, std::int32_t* status);
std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString7extractEiiPciNS0_10EInvariantE_nid_postfix(const Icu::Abi::UnicodeString* text, std::int32_t start, std::int32_t count, char* destination, std::int32_t capacity, std::int32_t);
void APS5_VABI _ZNK6icu_6813UnicodeString14extractBetweenEiiRS0__nid_postfix(const Icu::Abi::UnicodeString* text, std::int32_t start, std::int32_t limit, Icu::Abi::UnicodeString* target);
void APS5_VABI _ZN6icu_6813UnicodeString20handleReplaceBetweenEiiRKS0__nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t start, std::int32_t limit, const Icu::Abi::UnicodeString* source);
void APS5_VABI _ZN6icu_6813UnicodeString4copyEiii_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t start, std::int32_t limit, std::int32_t destination);
Icu::Abi::UnicodeString* APS5_VABI _ZNK6icu_6813UnicodeString13tempSubStringEii_nid_postfix(Icu::Abi::UnicodeString* result, const Icu::Abi::UnicodeString* text, std::int32_t start, std::int32_t count);
Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString14findAndReplaceEiiRKS0_iiS2_ii_nid_postfix(Icu::Abi::UnicodeString* text, std::int32_t start, std::int32_t count, const Icu::Abi::UnicodeString* oldText, std::int32_t oldStart, std::int32_t oldCount, const Icu::Abi::UnicodeString* replacement, std::int32_t newStart, std::int32_t newCount);
Icu::Abi::UnicodeString* APS5_VABI _ZNK6icu_6813UnicodeString8unescapeEv_nid_postfix(Icu::Abi::UnicodeString* result, const Icu::Abi::UnicodeString* text);
Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString9setToUTF8ENS_11StringPieceE_nid_postfix(Icu::Abi::UnicodeString* text, Icu::Abi::StringPiece input);
Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString8fromUTF8ENS_11StringPieceE_nid_postfix(Icu::Abi::UnicodeString* result, Icu::Abi::StringPiece input);
std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString6toUTF8EiiPci_nid_postfix(const Icu::Abi::UnicodeString* text, std::int32_t start, std::int32_t count, char* destination, std::int32_t capacity);
std::int32_t APS5_VABI _ZNK6icu_6813UnicodeString7toUTF32EPiiR10UErrorCode_nid_postfix(const Icu::Abi::UnicodeString* text, std::int32_t* destination, std::int32_t capacity, std::int32_t* status);
Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6813UnicodeString9fromUTF32EPKii_nid_postfix(Icu::Abi::UnicodeString* result, const std::int32_t* source, std::int32_t length);
}
