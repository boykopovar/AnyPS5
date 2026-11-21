#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

extern "C" {

void APS5_VABI _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(NKWebKit::WtfAbi::URL* self, const NKWebKit::WtfAbi::URL* base, const NKWebKit::WtfAbi::String* relative, const void* encoding);
bool APS5_VABI _ZN3WTF3URL11setProtocolENS_10StringViewE(NKWebKit::WtfAbi::URL* self, NKWebKit::WtfAbi::StringView protocol);
void APS5_VABI _ZN3WTF3URL7setHostENS_10StringViewE(NKWebKit::WtfAbi::URL* self, NKWebKit::WtfAbi::StringView host);
void APS5_VABI _ZN3WTF3URL14setHostAndPortENS_10StringViewE(NKWebKit::WtfAbi::URL* self, NKWebKit::WtfAbi::StringView hostAndPort);
void APS5_VABI _ZN3WTF3URL7setUserENS_10StringViewE(NKWebKit::WtfAbi::URL* self, NKWebKit::WtfAbi::StringView user);
void APS5_VABI _ZN3WTF3URL17removeCredentialsEv(NKWebKit::WtfAbi::URL* self);
void APS5_VABI _ZN3WTF3URL21setFragmentIdentifierENS_10StringViewE(NKWebKit::WtfAbi::URL* self, NKWebKit::WtfAbi::StringView identifier);
void APS5_VABI _ZN3WTF3URL8setQueryENS_10StringViewE(NKWebKit::WtfAbi::URL* self, NKWebKit::WtfAbi::StringView query);
void APS5_VABI _ZN3WTF3URL7setPathENS_10StringViewE(NKWebKit::WtfAbi::URL* self, NKWebKit::WtfAbi::StringView path);
NKWebKit::WtfAbi::URL APS5_VABI _ZNK3WTF3URL21truncatedForUseAsBaseEv(const NKWebKit::WtfAbi::URL* self);
NKWebKit::WtfAbi::URL APS5_VABI _ZN3WTF3URL25fileURLWithFileSystemPathENS_10StringViewE(NKWebKit::WtfAbi::StringView path);

}

namespace NKWebKit::WtfAbi {

void SetUrlPassword(URL* self, StringView password);

} // namespace NKWebKit::WtfAbi
