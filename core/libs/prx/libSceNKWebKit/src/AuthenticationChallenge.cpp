#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

const void* APS5_VABI WKAuthenticationChallengCopyRequestURL(const void* authenticationChallengeRef) {
    const auto* challenge = NKWebKit::Cast<NKWebKit::AuthenticationChallenge>(authenticationChallengeRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(challenge->requestURL));
}

const void* APS5_VABI WKAuthenticationChallengeGetDecisionListener(const void* authenticationChallengeRef) {
    const auto* challenge = NKWebKit::Cast<NKWebKit::AuthenticationChallenge>(authenticationChallengeRef, __func__);
    return challenge->decisionListener.Get();
}

int APS5_VABI WKAuthenticationChallengeGetPreviousFailureCount(const void* authenticationChallengeRef) {
    const auto* challenge = NKWebKit::Cast<NKWebKit::AuthenticationChallenge>(authenticationChallengeRef, __func__);
    return challenge->previousFailureCount;
}

const void* APS5_VABI WKAuthenticationChallengeGetProposedCredential(const void* authenticationChallengeRef) {
    const auto* challenge = NKWebKit::Cast<NKWebKit::AuthenticationChallenge>(authenticationChallengeRef, __func__);
    return challenge->proposedCredential.Get();
}

const void* APS5_VABI WKAuthenticationChallengeGetProtectionSpace(const void* authenticationChallengeRef) {
    const auto* challenge = NKWebKit::Cast<NKWebKit::AuthenticationChallenge>(authenticationChallengeRef, __func__);
    return challenge->protectionSpace.Get();
}

bool APS5_VABI WKAuthenticationChallengeIsForMainResource(const void* authenticationChallengeRef) {
    const auto* challenge = NKWebKit::Cast<NKWebKit::AuthenticationChallenge>(authenticationChallengeRef, __func__);
    return challenge->isForMainResource;
}

bool APS5_VABI WKAuthenticationChallengeIsInMainFrame(const void* authenticationChallengeRef) {
    const auto* challenge = NKWebKit::Cast<NKWebKit::AuthenticationChallenge>(authenticationChallengeRef, __func__);
    return challenge->isInMainFrame;
}

void APS5_VABI WKAuthenticationDecisionListenerCancel(const void* decisionListenerRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::AuthenticationDecisionListener>(decisionListenerRef, __func__);
    listener->decision = NKWebKit::AuthenticationDecisionListener::Decision::Cancel;
}

void APS5_VABI WKAuthenticationDecisionListenerRejectProtectionSpaceAndContinue(const void* decisionListenerRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::AuthenticationDecisionListener>(decisionListenerRef, __func__);
    listener->decision = NKWebKit::AuthenticationDecisionListener::Decision::RejectProtectionSpace;
}

void APS5_VABI WKAuthenticationDecisionListenerUseCredential(const void* decisionListenerRef, const void* credentialRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::AuthenticationDecisionListener>(decisionListenerRef, __func__);
    listener->decision = NKWebKit::AuthenticationDecisionListener::Decision::UseCredential;
    auto* cred = NKWebKit::CastOrNull<NKWebKit::ObjectOf<NKWebKit::ObjectType::Credential>>(credentialRef, __func__);
    listener->credential = NKWebKit::Retained<NKWebKit::Object>::Share(cred);
}

}
