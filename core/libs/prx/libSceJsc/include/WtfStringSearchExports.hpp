#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>

extern "C" {

bool APS5_VABI _ZNK3WTF10StringImpl10startsWithEPKS0_(const Jsc::WtfAbi::StringImpl* self, const Jsc::WtfAbi::StringImpl* prefix);
bool APS5_VABI _ZNK3WTF10StringImpl10startsWithERKS0_(const Jsc::WtfAbi::StringImpl* self, const Jsc::WtfAbi::StringImpl* prefix);
bool APS5_VABI _ZNK3WTF10StringImpl27startsWithIgnoringASCIICaseEPKS0_(const Jsc::WtfAbi::StringImpl* self, const Jsc::WtfAbi::StringImpl* prefix);
bool APS5_VABI _ZNK3WTF10StringImpl27startsWithIgnoringASCIICaseERKS0_(const Jsc::WtfAbi::StringImpl* self, const Jsc::WtfAbi::StringImpl* prefix);
bool APS5_VABI _ZN3WTF10StringImpl8endsWithEPS0_(Jsc::WtfAbi::StringImpl* self, Jsc::WtfAbi::StringImpl* suffix);
bool APS5_VABI _ZN3WTF10StringImpl8endsWithERS0_(Jsc::WtfAbi::StringImpl* self, Jsc::WtfAbi::StringImpl* suffix);
bool APS5_VABI _ZNK3WTF10StringImpl25endsWithIgnoringASCIICaseEPKS0_(const Jsc::WtfAbi::StringImpl* self, const Jsc::WtfAbi::StringImpl* suffix);
bool APS5_VABI _ZNK3WTF10StringImpl25endsWithIgnoringASCIICaseERKS0_(const Jsc::WtfAbi::StringImpl* self, const Jsc::WtfAbi::StringImpl* suffix);
bool APS5_VABI _ZNK3WTF10StringImpl18hasInfixStartingAtERKS0_j(const Jsc::WtfAbi::StringImpl* self, const Jsc::WtfAbi::StringImpl* match, unsigned startOffset);
bool APS5_VABI _ZNK3WTF10StringImpl16hasInfixEndingAtERKS0_j(const Jsc::WtfAbi::StringImpl* self, const Jsc::WtfAbi::StringImpl* match, unsigned endOffset);
std::size_t APS5_VABI _ZN3WTF10StringImpl4findEPFbDsEj(Jsc::WtfAbi::StringImpl* self, Jsc::WtfAbi::CodeUnitMatchFunction matchFunction, unsigned start);
std::size_t APS5_VABI _ZN3WTF10StringImpl4findEPS0_(Jsc::WtfAbi::StringImpl* self, Jsc::WtfAbi::StringImpl* match);
std::size_t APS5_VABI _ZN3WTF10StringImpl4findEPS0_j(Jsc::WtfAbi::StringImpl* self, Jsc::WtfAbi::StringImpl* match, unsigned index);
std::size_t APS5_VABI _ZN3WTF10StringImpl11reverseFindEPS0_j(Jsc::WtfAbi::StringImpl* self, Jsc::WtfAbi::StringImpl* match, unsigned index);
std::size_t APS5_VABI _ZNK3WTF10StringImpl21findIgnoringASCIICaseEPKS0_(const Jsc::WtfAbi::StringImpl* self, const Jsc::WtfAbi::StringImpl* match);
std::size_t APS5_VABI _ZNK3WTF10StringImpl21findIgnoringASCIICaseEPKS0_j(const Jsc::WtfAbi::StringImpl* self, const Jsc::WtfAbi::StringImpl* match, unsigned startOffset);
std::size_t APS5_VABI _ZNK3WTF10StringImpl21findIgnoringASCIICaseERKS0_(const Jsc::WtfAbi::StringImpl* self, const Jsc::WtfAbi::StringImpl* match);
std::size_t APS5_VABI _ZNK3WTF10StringImpl21findIgnoringASCIICaseERKS0_j(const Jsc::WtfAbi::StringImpl* self, const Jsc::WtfAbi::StringImpl* match, unsigned startOffset);
bool APS5_VABI _ZNK3WTF10StringView8containsEPKc(const Jsc::WtfAbi::StringView* self, const char* string);
bool APS5_VABI _ZN3WTF10protocolIsENS_10StringViewEPKc(Jsc::WtfAbi::StringView string, const char* protocol);
std::int32_t APS5_VABI _ZN3WTF10StringImpl23defaultWritingDirectionEPb(Jsc::WtfAbi::StringImpl* self, bool* hasStrongDirectionality);

}
