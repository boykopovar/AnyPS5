#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/AuthenticationChallengeAbi.hpp"

extern "C" {

void* APS5_VABI _ZNK7WebCore23AuthenticationChallenge20authenticationClientEv(const NKWebKit::WebCoreAbi::AuthenticationChallenge* self);
bool APS5_VABI _ZN7WebCore23AuthenticationChallenge15platformCompareERKS0_S2_(const NKWebKit::WebCoreAbi::AuthenticationChallenge* a, const NKWebKit::WebCoreAbi::AuthenticationChallenge* b);
bool APS5_VABI _ZNK7WebCore27AuthenticationChallengeBase6isNullEv(const NKWebKit::WebCoreAbi::AuthenticationChallenge* self);
void APS5_VABI _ZN7WebCore27AuthenticationChallengeBase7nullifyEv(NKWebKit::WebCoreAbi::AuthenticationChallenge* self);

}
