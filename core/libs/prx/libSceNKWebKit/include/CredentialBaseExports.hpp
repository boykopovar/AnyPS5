#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/CredentialBaseAbi.hpp"

extern "C" {

bool APS5_VABI _ZN7WebCore14CredentialBase7compareERKNS_10CredentialES3_(const NKWebKit::WebCoreAbi::CredentialBase* a, const NKWebKit::WebCoreAbi::CredentialBase* b);
void APS5_VABI _ZN7WebCore14CredentialBaseC1ERKN3WTF6StringES4_NS_21CredentialPersistenceE(NKWebKit::WebCoreAbi::CredentialBase* self, const NKWebKit::WtfAbi::String* user, const NKWebKit::WtfAbi::String* password, std::uint32_t persistence);
void APS5_VABI _ZN7WebCore14CredentialBaseC1Ev(NKWebKit::WebCoreAbi::CredentialBase* self);
void APS5_VABI _ZN7WebCore14CredentialBaseC2ERKN3WTF6StringES4_NS_21CredentialPersistenceE(NKWebKit::WebCoreAbi::CredentialBase* self, const NKWebKit::WtfAbi::String* user, const NKWebKit::WtfAbi::String* password, std::uint32_t persistence);
void APS5_VABI _ZN7WebCore14CredentialBaseC2Ev(NKWebKit::WebCoreAbi::CredentialBase* self);
bool APS5_VABI _ZNK7WebCore14CredentialBase11hasPasswordEv(const NKWebKit::WebCoreAbi::CredentialBase* self);
std::uint32_t APS5_VABI _ZNK7WebCore14CredentialBase11persistenceEv(const NKWebKit::WebCoreAbi::CredentialBase* self);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore14CredentialBase4userEv(const NKWebKit::WebCoreAbi::CredentialBase* self);
bool APS5_VABI _ZNK7WebCore14CredentialBase7isEmptyEv(const NKWebKit::WebCoreAbi::CredentialBase* self);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore14CredentialBase8passwordEv(const NKWebKit::WebCoreAbi::CredentialBase* self);

}
