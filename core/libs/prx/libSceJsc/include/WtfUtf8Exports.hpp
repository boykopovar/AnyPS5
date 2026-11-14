#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/src/WtfStringAbi.hpp"

#include <cstddef>

namespace Jsc::WtfAbi {

enum ConversionResult : int {
    ConversionOK,
    SourceExhausted,
    TargetExhausted,
    SourceIllegal,
};

static_assert(sizeof(ConversionResult) == 4);

}  // namespace Jsc::WtfAbi

extern "C" {

bool APS5_VABI _ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_(const unsigned char** sourceStart, const unsigned char* sourceEnd, char** targetStart, char* targetEnd);
Jsc::WtfAbi::ConversionResult APS5_VABI _ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(const char16_t** sourceStart, const char16_t* sourceEnd, char** targetStart, char* targetEnd, bool strict);
bool APS5_VABI _ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(const char* source, const char* sourceEnd, char16_t** targetStart, char16_t* targetEnd, bool* sourceAllASCII);
bool APS5_VABI _ZN3WTF7Unicode43convertUTF8ToUTF16ReplacingInvalidSequencesEPKcS2_PPDsS3_Pb(const char* source, const char* sourceEnd, char16_t** targetStart, char16_t* targetEnd, bool* sourceAllASCII);
Jsc::WtfAbi::UTF8ConversionError APS5_VABI _ZN3WTF10StringImpl8utf8ImplEPKDsjRPcmNS_14ConversionModeE(const char16_t* characters, unsigned length, char*& buffer, std::size_t bufferSize, Jsc::WtfAbi::ConversionMode mode);
Jsc::WtfAbi::ExpectedCString APS5_VABI _ZNK3WTF10StringImpl10tryGetUtf8ENS_14ConversionModeE(const Jsc::WtfAbi::StringImpl* self, Jsc::WtfAbi::ConversionMode mode);
Jsc::WtfAbi::ExpectedCString APS5_VABI _ZNK3WTF10StringImpl18tryGetUtf8ForRangeEjjNS_14ConversionModeE(const Jsc::WtfAbi::StringImpl* self, unsigned offset, unsigned length, Jsc::WtfAbi::ConversionMode mode);
Jsc::WtfAbi::ExpectedCString APS5_VABI _ZNK3WTF10StringView10tryGetUtf8ENS_14ConversionModeE(const Jsc::WtfAbi::StringView* self, Jsc::WtfAbi::ConversionMode mode);
Jsc::WtfAbi::ExpectedCString APS5_VABI _ZNK3WTF6String10tryGetUtf8ENS_14ConversionModeE(const Jsc::WtfAbi::String* self, Jsc::WtfAbi::ConversionMode mode);
Jsc::WtfAbi::ExpectedCString APS5_VABI _ZNK3WTF6String10tryGetUtf8Ev(const Jsc::WtfAbi::String* self);
Jsc::WtfAbi::CString APS5_VABI _ZNK3WTF6String4utf8Ev(const Jsc::WtfAbi::String* self);
}
