#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfValueExports.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstdint>

extern "C" {

NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF14AtomStringImpl3addEPKhj(const unsigned char* characters, unsigned length);
NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF14AtomStringImpl3addEPKDsj(const char16_t* characters, unsigned length);
NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF14AtomStringImpl3addEPNS_10StringImplEjj(NKWebKit::WtfAbi::StringImpl* baseString, unsigned start, unsigned length);
NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF14AtomStringImpl3addEPKNS_10StringImpl16StaticStringImplE(const NKWebKit::WtfAbi::StringImpl* string);
NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF14AtomStringImpl10addLiteralEPKcj(const char* characters, unsigned length);
NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF14AtomStringImpl11addSlowCaseERNS_10StringImplE(NKWebKit::WtfAbi::StringImpl* string);
NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF14AtomStringImpl11addSlowCaseERNS_15AtomStringTableERNS_10StringImplE(void* table, NKWebKit::WtfAbi::StringImpl* string);
NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF14AtomStringImpl14lookUpSlowCaseERNS_10StringImplE(NKWebKit::WtfAbi::StringImpl* string);
NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF14AtomStringImpl6lookUpEPKhj(const unsigned char* characters, unsigned length);
NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF14AtomStringImpl6lookUpEPKDsj(const char16_t* characters, unsigned length);
NKWebKit::WtfAbi::AtomString APS5_VABI _ZN3WTF10AtomString16fromUTF8InternalEPKcS2_(const char* start, const char* end);
NKWebKit::WtfAbi::AtomString APS5_VABI _ZN3WTF10AtomString6numberEi(int number);
NKWebKit::WtfAbi::AtomString APS5_VABI _ZN3WTF10AtomString6numberEj(unsigned number);
NKWebKit::WtfAbi::AtomString APS5_VABI _ZN3WTF10AtomString6numberEm(std::uint64_t number);
NKWebKit::WtfAbi::AtomString APS5_VABI _ZN3WTF10AtomString6numberEy(unsigned long long number);
NKWebKit::WtfAbi::AtomString APS5_VABI _ZN3WTF10AtomString6numberEf(float number);
NKWebKit::WtfAbi::AtomString APS5_VABI _ZN3WTF10AtomString6numberEd(double number);
NKWebKit::WtfAbi::AtomString APS5_VABI _ZNK3WTF10AtomString23convertToASCIILowercaseEv(const NKWebKit::WtfAbi::AtomString* self);
NKWebKit::WtfAbi::AtomString APS5_VABI _ZNK3WTF10AtomString23convertToASCIIUppercaseEv(const NKWebKit::WtfAbi::AtomString* self);
void APS5_VABI _ZN3WTF10StringImpl7destroyEPS0_(NKWebKit::WtfAbi::StringImpl* stringImpl);

}
