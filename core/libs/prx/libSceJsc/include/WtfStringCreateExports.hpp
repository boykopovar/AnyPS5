#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/src/WtfStringAbi.hpp"

#include <cstddef>

extern "C" {

Jsc::WtfAbi::String APS5_VABI _ZN3WTF10StringImpl6createEPKh(const unsigned char* string);
Jsc::WtfAbi::String APS5_VABI _ZN3WTF10StringImpl20create8BitIfPossibleEPKDs(const char16_t* string);
Jsc::WtfAbi::String APS5_VABI _ZN3WTF10StringImpl17createFromLiteralEPKc(const char* characters);
Jsc::WtfAbi::String APS5_VABI _ZN3WTF10StringImpl17createFromLiteralEPKcj(const char* characters, unsigned length);
Jsc::WtfAbi::String APS5_VABI _ZN3WTF10StringImpl20createWithoutCopyingEPKDsj(const char16_t* characters, unsigned length);
Jsc::WtfAbi::String APS5_VABI _ZN3WTF10StringImpl20createWithoutCopyingEPKhj(const unsigned char* characters, unsigned length);
Jsc::WtfAbi::String APS5_VABI _ZN3WTF10StringImpl7replaceEDsPS0_(Jsc::WtfAbi::StringImpl* self, char16_t pattern, Jsc::WtfAbi::StringImpl* replacement);
Jsc::WtfAbi::String APS5_VABI _ZN3WTF10StringImpl7replaceEPS0_S1_(Jsc::WtfAbi::StringImpl* self, Jsc::WtfAbi::StringImpl* pattern, Jsc::WtfAbi::StringImpl* replacement);
Jsc::WtfAbi::String APS5_VABI _ZN3WTF10StringImpl7replaceEjjPS0_(Jsc::WtfAbi::StringImpl* self, unsigned position, unsigned lengthToReplace, Jsc::WtfAbi::StringImpl* string);
Jsc::WtfAbi::String APS5_VABI _ZN3WTF10StringImpl18simplifyWhiteSpaceEv(Jsc::WtfAbi::StringImpl* self);
void APS5_VABI _ZN3WTF6StringC1ENS_12ASCIILiteralE(Jsc::WtfAbi::String* self, Jsc::WtfAbi::ASCIILiteral characters);
void APS5_VABI _ZN3WTF6StringC2ENS_12ASCIILiteralE(Jsc::WtfAbi::String* self, Jsc::WtfAbi::ASCIILiteral characters);
void APS5_VABI _ZN3WTF6StringC1EPKDs(Jsc::WtfAbi::String* self, const char16_t* string);
void APS5_VABI _ZN3WTF6StringC2EPKDs(Jsc::WtfAbi::String* self, const char16_t* string);
void APS5_VABI _ZN3WTF6StringC1EPKh(Jsc::WtfAbi::String* self, const unsigned char* string);
void APS5_VABI _ZN3WTF6StringC2EPKh(Jsc::WtfAbi::String* self, const unsigned char* string);
Jsc::WtfAbi::String APS5_VABI _ZN3WTF6String23make16BitFrom8BitSourceEPKhm(const unsigned char* source, std::size_t length);
Jsc::WtfAbi::String APS5_VABI _ZN3WTF6String23make8BitFrom16BitSourceEPKDsm(const char16_t* source, std::size_t length);
void APS5_VABI _ZN3WTF6String6appendEDs(Jsc::WtfAbi::String* self, char16_t character);
void APS5_VABI _ZN3WTF6String6appendEPKDsj(Jsc::WtfAbi::String* self, const char16_t* characters, unsigned length);
void APS5_VABI _ZN3WTF6String6appendEPKhj(Jsc::WtfAbi::String* self, const unsigned char* characters, unsigned length);
void APS5_VABI _ZN3WTF6String6appendERKS0_(Jsc::WtfAbi::String* self, const Jsc::WtfAbi::String* string);
void APS5_VABI _ZN3WTF6String6appendEh(Jsc::WtfAbi::String* self, unsigned char character);
void APS5_VABI _ZN3WTF6String6insertERKS0_j(Jsc::WtfAbi::String* self, const Jsc::WtfAbi::String* string, unsigned position);
void APS5_VABI _ZN3WTF6String6removeEjj(Jsc::WtfAbi::String* self, unsigned position, unsigned lengthToRemove);
void APS5_VABI _ZN3WTF6String8truncateEj(Jsc::WtfAbi::String* self, unsigned length);
Jsc::WtfAbi::String APS5_VABI _ZNK3WTF6String9substringEjj(const Jsc::WtfAbi::String* self, unsigned position, unsigned length);
Jsc::WtfAbi::String APS5_VABI _ZNK3WTF6String15stripWhiteSpaceEv(const Jsc::WtfAbi::String* self);
Jsc::WtfAbi::String APS5_VABI _ZNK3WTF6String18simplifyWhiteSpaceEv(const Jsc::WtfAbi::String* self);
Jsc::WtfAbi::String APS5_VABI _ZNK3WTF6String33stripLeadingAndTrailingCharactersEPFbDsE(const Jsc::WtfAbi::String* self, Jsc::WtfAbi::CodeUnitMatchFunction predicate);
Jsc::WtfAbi::StringVector APS5_VABI _ZNK3WTF6String5splitERKS0_(const Jsc::WtfAbi::String* self, const Jsc::WtfAbi::String* separator);
Jsc::WtfAbi::StringVector APS5_VABI _ZNK3WTF6String25splitAllowingEmptyEntriesERKS0_(const Jsc::WtfAbi::String* self, const Jsc::WtfAbi::String* separator);
unsigned APS5_VABI _ZNK3WTF6String12toUIntStrictEPbi(const Jsc::WtfAbi::String* self, bool* ok, int base);
const Jsc::WtfAbi::String& APS5_VABI _ZN3WTF10nullStringEv();
const Jsc::WtfAbi::String& APS5_VABI _ZN3WTF11emptyStringEv();

}
