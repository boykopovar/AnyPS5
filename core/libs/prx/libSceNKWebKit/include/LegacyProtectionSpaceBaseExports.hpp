#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/LegacyProtectionSpaceBaseAbi.hpp"
#include <cstdint>

extern "C" {

void APS5_VABI _ZN7WebCore19ProtectionSpaceBaseC1Ev(NKWebKit::WebCoreAbi::LegacyProtectionSpaceBase* self);
void APS5_VABI _ZN7WebCore19ProtectionSpaceBaseC2Ev(NKWebKit::WebCoreAbi::LegacyProtectionSpaceBase* self);
void APS5_VABI _ZN7WebCore19ProtectionSpaceBaseC1ERKN3WTF6StringEiNS_25ProtectionSpaceServerTypeES4_NS_35ProtectionSpaceAuthenticationSchemeE(NKWebKit::WebCoreAbi::LegacyProtectionSpaceBase* self, const NKWebKit::WtfAbi::String* host, std::int32_t port, std::uint32_t serverType, const NKWebKit::WtfAbi::String* realm, std::uint32_t authenticationScheme);
void APS5_VABI _ZN7WebCore19ProtectionSpaceBaseC2ERKN3WTF6StringEiNS_25ProtectionSpaceServerTypeES4_NS_35ProtectionSpaceAuthenticationSchemeE(NKWebKit::WebCoreAbi::LegacyProtectionSpaceBase* self, const NKWebKit::WtfAbi::String* host, std::int32_t port, std::uint32_t serverType, const NKWebKit::WtfAbi::String* realm, std::uint32_t authenticationScheme);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore19ProtectionSpaceBase4hostEv(const NKWebKit::WebCoreAbi::LegacyProtectionSpaceBase* self);
std::int32_t APS5_VABI _ZNK7WebCore19ProtectionSpaceBase4portEv(const NKWebKit::WebCoreAbi::LegacyProtectionSpaceBase* self);
std::uint32_t APS5_VABI _ZNK7WebCore19ProtectionSpaceBase10serverTypeEv(const NKWebKit::WebCoreAbi::LegacyProtectionSpaceBase* self);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore19ProtectionSpaceBase5realmEv(const NKWebKit::WebCoreAbi::LegacyProtectionSpaceBase* self);
std::uint32_t APS5_VABI _ZNK7WebCore19ProtectionSpaceBase20authenticationSchemeEv(const NKWebKit::WebCoreAbi::LegacyProtectionSpaceBase* self);

}
