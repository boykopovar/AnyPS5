#pragma once

#include "prx/libSceJsc/include/WtfUnicodeExports.hpp"

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/src/WtfStringAbi.hpp"

#include <cstddef>

extern "C" {

Jsc::WtfAbi::UTF8ConversionError APS5_VABI _ZN3WTF10StringImpl8utf8ImplEPKDsjRPcmNS_14ConversionModeE(const char16_t* characters, unsigned length, char*& buffer, std::size_t bufferSize, Jsc::WtfAbi::ConversionMode mode);
Jsc::WtfAbi::ExpectedCString APS5_VABI _ZNK3WTF10StringImpl10tryGetUtf8ENS_14ConversionModeE(const Jsc::WtfAbi::StringImpl* self, Jsc::WtfAbi::ConversionMode mode);
Jsc::WtfAbi::ExpectedCString APS5_VABI _ZNK3WTF10StringImpl18tryGetUtf8ForRangeEjjNS_14ConversionModeE(const Jsc::WtfAbi::StringImpl* self, unsigned offset, unsigned length, Jsc::WtfAbi::ConversionMode mode);
Jsc::WtfAbi::ExpectedCString APS5_VABI _ZNK3WTF10StringView10tryGetUtf8ENS_14ConversionModeE(const Jsc::WtfAbi::StringView* self, Jsc::WtfAbi::ConversionMode mode);
Jsc::WtfAbi::ExpectedCString APS5_VABI _ZNK3WTF6String10tryGetUtf8ENS_14ConversionModeE(const Jsc::WtfAbi::String* self, Jsc::WtfAbi::ConversionMode mode);
Jsc::WtfAbi::ExpectedCString APS5_VABI _ZNK3WTF6String10tryGetUtf8Ev(const Jsc::WtfAbi::String* self);
Jsc::WtfAbi::OwnedCString APS5_VABI _ZNK3WTF6String4utf8Ev(const Jsc::WtfAbi::String* self);
}
