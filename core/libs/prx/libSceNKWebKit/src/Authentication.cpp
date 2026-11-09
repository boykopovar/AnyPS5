#include "prx/libSceNKWebKit/src/Authentication.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

const void* APS5_VABI WKAuthenticationChallengCopyRequestURL(const void* challengeRef) {
    const auto* challenge = NKWebKit::Cast<NKWebKit::AuthenticationChallenge>(challengeRef, __func__);
    if (!challenge->requestURL) return nullptr;
    return NKWebKit::ToRef(NKWebKit::Retained<NKWebKit::URL>::Share(challenge->requestURL.Get()));
}

const void* APS5_VABI WKAuthenticationChallengeGetDecisionListener(const void* challengeRef) {
    auto* challenge = NKWebKit::Cast<NKWebKit::AuthenticationChallenge>(challengeRef, __func__);
    return challenge->GetOrCreateDecisionListener();
}

std::uint32_t APS5_VABI WKAuthenticationChallengeGetPreviousFailureCount(const void* challengeRef) {
    const auto* challenge = NKWebKit::Cast<NKWebKit::AuthenticationChallenge>(challengeRef, __func__);
    return challenge->previousFailureCount;
}

const void* APS5_VABI WKAuthenticationChallengeGetProposedCredential(const void* challengeRef) {
    const auto* challenge = NKWebKit::Cast<NKWebKit::AuthenticationChallenge>(challengeRef, __func__);
    return challenge->proposedCredential ? challenge->proposedCredential.Get() : nullptr;
}

const void* APS5_VABI WKAuthenticationChallengeGetProtectionSpace(const void* challengeRef) {
    const auto* challenge = NKWebKit::Cast<NKWebKit::AuthenticationChallenge>(challengeRef, __func__);
    return challenge->protectionSpace ? challenge->protectionSpace.Get() : nullptr;
}

bool APS5_VABI WKAuthenticationChallengeIsForMainResource(const void* challengeRef) {
    const auto* challenge = NKWebKit::Cast<NKWebKit::AuthenticationChallenge>(challengeRef, __func__);
    return challenge->isForMainResource;
}

bool APS5_VABI WKAuthenticationChallengeIsInMainFrame(const void* challengeRef) {
    const auto* challenge = NKWebKit::Cast<NKWebKit::AuthenticationChallenge>(challengeRef, __func__);
    return challenge->isInMainFrame;
}

void APS5_VABI WKAuthenticationDecisionListenerCancel(const void* listenerRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::AuthenticationDecisionListener>(listenerRef, __func__);
    listener->decision = NKWebKit::AuthenticationDecisionListener::Decision::Cancel;
    listener->credential = nullptr;
}

void APS5_VABI WKAuthenticationDecisionListenerRejectProtectionSpaceAndContinue(const void* listenerRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::AuthenticationDecisionListener>(listenerRef, __func__);
    listener->decision = NKWebKit::AuthenticationDecisionListener::Decision::RejectProtectionSpaceAndContinue;
    listener->credential = nullptr;
}

void APS5_VABI WKAuthenticationDecisionListenerUseCredential(const void* listenerRef, const void* credentialRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::AuthenticationDecisionListener>(listenerRef, __func__);
    listener->decision = NKWebKit::AuthenticationDecisionListener::Decision::UseCredential;
    if (credentialRef) {
        const auto* obj = NKWebKit::ToObject(credentialRef, __func__);
        if (obj->Type() != NKWebKit::ObjectType::Credential) {
            throw std::invalid_argument(std::string(__func__) + ": object reference of another type");
        }
        listener->credential = NKWebKit::Retained<NKWebKit::Object>::Share(const_cast<NKWebKit::Object*>(obj));
    } else {
        listener->credential = nullptr;
    }
}

}
