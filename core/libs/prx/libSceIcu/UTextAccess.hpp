#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {

struct UTextFuncs;

struct UText {
    std::uint32_t magic;
    std::int32_t flags;
    std::int32_t providerProperties;
    std::int32_t sizeOfStruct;
    std::int64_t chunkNativeLimit;
    std::int32_t extraSize;
    std::int32_t nativeIndexingLimit;
    std::int64_t chunkNativeStart;
    std::int32_t chunkOffset;
    std::int32_t chunkLength;
    const char16_t* chunkContents;
    const UTextFuncs* pFuncs;
    UText* pExtra;
    const void* context;
    const void* p;
    const void* q;
    const void* r;
    void* privP;
    std::int64_t a;
    std::int32_t b;
    std::int32_t c;
    std::int64_t privA;
    std::int32_t privB;
    std::int32_t privC;
};

using UTextClone = UText* (APS5_VABI*)(UText*, const UText*, std::int8_t, std::int32_t*);
using UTextNativeLength = std::int64_t (APS5_VABI*)(UText*);
using UTextAccess = std::int8_t (APS5_VABI*)(UText*, std::int64_t, std::int8_t);
using UTextExtract = std::int32_t (APS5_VABI*)(UText*, std::int64_t, std::int64_t, char16_t*, std::int32_t,
    std::int32_t*);
using UTextReplace = std::int32_t (APS5_VABI*)(UText*, std::int64_t, std::int64_t, const char16_t*, std::int32_t,
    std::int32_t*);
using UTextCopy = void (APS5_VABI*)(UText*, std::int64_t, std::int64_t, std::int64_t, std::int8_t, std::int32_t*);
using UTextMapOffsetToNative = std::int64_t (APS5_VABI*)(const UText*);
using UTextMapNativeIndexToUTF16 = std::int32_t (APS5_VABI*)(const UText*, std::int64_t);
using UTextClose = void (APS5_VABI*)(UText*);

struct UTextFuncs {
    std::int32_t tableSize;
    std::int32_t reserved1;
    std::int32_t reserved2;
    std::int32_t reserved3;
    UTextClone clone;
    UTextNativeLength nativeLength;
    UTextAccess access;
    UTextExtract extract;
    UTextReplace replace;
    UTextCopy copy;
    UTextMapOffsetToNative mapOffsetToNative;
    UTextMapNativeIndexToUTF16 mapNativeIndexToUTF16;
    UTextClose close;
    void* spare1;
    void* spare2;
    void* spare3;
};

UText* APS5_VABI utext_openUChars_nid_postfix(UText* text, const char16_t* source, std::int64_t length,
                                               std::int32_t* status);
UText* APS5_VABI utext_setup_nid_postfix(UText* text, std::int32_t extraSpace, std::int32_t* status);
UText* APS5_VABI utext_clone_nid_postfix(UText* destination, const UText* source, std::int8_t deep,
                                          std::int8_t readOnly, std::int32_t* status);
std::int8_t APS5_VABI utext_equals_nid_postfix(const UText* first, const UText* second);
std::int64_t APS5_VABI utext_getPreviousNativeIndex_nid_postfix(UText* text);
std::int32_t APS5_VABI utext_next32From_nid_postfix(UText* text, std::int64_t index);
std::int32_t APS5_VABI utext_current32_nid_postfix(UText* text);
void APS5_VABI utext_copy_nid_postfix(UText* text, std::int64_t nativeStart, std::int64_t nativeLimit,
                                      std::int64_t destinationIndex, std::int8_t move, std::int32_t* status);
std::int32_t APS5_VABI utext_char32At_nid_postfix(UText* text, std::int64_t nativeIndex);
std::int32_t APS5_VABI utext_next32_nid_postfix(UText* text);
std::int32_t APS5_VABI utext_replace_nid_postfix(UText* text, std::int64_t nativeStart, std::int64_t nativeLimit,
                                                 const char16_t* replacement, std::int32_t replacementLength,
                                                 std::int32_t* status);
std::int8_t APS5_VABI utext_moveIndex32_nid_postfix(UText* text, std::int32_t delta);
std::int8_t APS5_VABI utext_isLengthExpensive_nid_postfix(const UText* text);
std::int8_t APS5_VABI utext_isWritable_nid_postfix(const UText* text);
std::int32_t APS5_VABI utext_previous32_nid_postfix(UText* text);
std::int32_t APS5_VABI utext_extract_nid_postfix(UText* text, std::int64_t start, std::int64_t limit,
                                                 char16_t* destination, std::int32_t destinationCapacity,
                                                 std::int32_t* status);
void APS5_VABI utext_setNativeIndex_nid_postfix(UText* text, std::int64_t index);
std::int32_t APS5_VABI utext_previous32From_nid_postfix(UText* text, std::int64_t index);
std::int64_t APS5_VABI utext_getNativeIndex_nid_postfix(const UText* text);
void APS5_VABI utext_freeze_nid_postfix(UText* text);
std::int64_t APS5_VABI utext_nativeLength_nid_postfix(UText* text);
std::int8_t APS5_VABI utext_hasMetaData_nid_postfix(const UText* text);

}
