#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

extern "C" {

bool APS5_VABI _ZN7WebCore23wrapSerializedCryptoKeyERKN3WTF6VectorIhLm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEES6_RS4_(const NKWebKit::WtfAbi::Vector<unsigned char>& masterKey, const NKWebKit::WtfAbi::Vector<unsigned char>& key, NKWebKit::WtfAbi::Vector<unsigned char>& result);
bool APS5_VABI _ZN7WebCore25unwrapSerializedCryptoKeyERKN3WTF6VectorIhLm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEES6_RS4_(const NKWebKit::WtfAbi::Vector<unsigned char>& masterKey, const NKWebKit::WtfAbi::Vector<unsigned char>& wrappedKey, NKWebKit::WtfAbi::Vector<unsigned char>& key);

}
