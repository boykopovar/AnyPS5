#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/LoggedInStatusAbi.hpp"

#include <cstdint>

extern "C" {

NKWebKit::WebCoreAbi::LoggedInStatusOrException APS5_VABI _ZN7WebCore14LoggedInStatus6createERKNS_17RegistrableDomainERKN3WTF6StringENS0_19CredentialTokenTypeENS0_18AuthenticationTypeE(const NKWebKit::WebCoreAbi::RegistrableDomain* domain, const NKWebKit::WtfAbi::String* username, bool tokenType, std::uint8_t authType);
NKWebKit::WebCoreAbi::LoggedInStatusOrException APS5_VABI _ZN7WebCore14LoggedInStatus6createERKNS_17RegistrableDomainERKN3WTF6StringENS0_19CredentialTokenTypeENS0_18AuthenticationTypeENS4_7SecondsE(const NKWebKit::WebCoreAbi::RegistrableDomain* domain, const NKWebKit::WtfAbi::String* username, bool tokenType, std::uint8_t authType, WTF::Seconds timeToLive);
void APS5_VABI _ZN7WebCore14LoggedInStatus13setTimeToLiveEN3WTF7SecondsE(NKWebKit::WebCoreAbi::LoggedInStatus* self, WTF::Seconds timeToLive);
bool APS5_VABI _ZNK7WebCore14LoggedInStatus10hasExpiredEv(const NKWebKit::WebCoreAbi::LoggedInStatus* self);
WTF::WallTime APS5_VABI _ZNK7WebCore14LoggedInStatus6expiryEv(const NKWebKit::WebCoreAbi::LoggedInStatus* self);

}
