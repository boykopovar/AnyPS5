#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/src/WtfHelperAbi.hpp"

#include <cstdint>

extern "C" {

Jsc::WtfAbi::SixCharacterHash APS5_VABI _ZN3WTF31integerToSixCharacterHashStringEj(unsigned hash);
Jsc::WtfAbi::HexResult APS5_VABI _ZN3WTF8Internal9appendHexEPhjmjNS_17HexConversionModeE(Jsc::WtfAbi::LChar* buffer, unsigned bufferSize, std::uintmax_t number, unsigned minimumDigits, Jsc::WtfAbi::HexConversionMode mode);
bool APS5_VABI _ZNK3WTF9MediaTimeneERKS0_(const Jsc::WtfAbi::MediaTime* self, const Jsc::WtfAbi::MediaTime* rhs);

}
