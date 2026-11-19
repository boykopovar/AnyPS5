#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

namespace Jsc::WtfAbi {

enum class ConversionResult : int {
    ConversionOK,
    SourceExhausted,
    TargetExhausted,
    SourceIllegal,
};

}

extern "C" {

bool APS5_VABI _ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_(const unsigned char** sourceStart, const unsigned char* sourceEnd, char** targetStart, char* targetEnd);
Jsc::WtfAbi::ConversionResult APS5_VABI _ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(const char16_t** sourceStart, const char16_t* sourceEnd, char** targetStart, char* targetEnd, bool strict);
bool APS5_VABI _ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(const char* source, const char* sourceEnd, char16_t** targetStart, char16_t* targetEnd, bool* sourceAllAscii);
bool APS5_VABI _ZN3WTF7Unicode43convertUTF8ToUTF16ReplacingInvalidSequencesEPKcS2_PPDsS3_Pb(const char* source, const char* sourceEnd, char16_t** targetStart, char16_t* targetEnd, bool* sourceAllAscii);

}
