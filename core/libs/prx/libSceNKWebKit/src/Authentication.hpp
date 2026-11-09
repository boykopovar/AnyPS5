#pragma once

#include "prx/libSceNKWebKit/src/Object.hpp"
#include "prx/libSceNKWebKit/src/ProtectionSpace.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include <cstdint>
#include <utility>

namespace NKWebKit {

class AuthenticationDecisionListener final : public ObjectOf<ObjectType::AuthenticationDecisionListener> {
public:
    enum class Decision {
        Pending,
        UseCredential,
        Cancel,
        RejectProtectionSpaceAndContinue,
    };

    Decision decision = Decision::Pending;
    Retained<Object> credential;
};

class AuthenticationChallenge final : public ObjectOf<ObjectType::AuthenticationChallenge> {
public:
    Retained<ProtectionSpace> protectionSpace;
    Retained<Object> proposedCredential;
    std::uint32_t previousFailureCount = 0;
    Retained<URL> requestURL;
    bool isForMainResource = true;
    bool isInMainFrame = true;
    Retained<AuthenticationDecisionListener> decisionListener;

    AuthenticationDecisionListener* GetOrCreateDecisionListener() {
        if (!decisionListener) {
            decisionListener = Make<AuthenticationDecisionListener>();
        }
        return decisionListener.Get();
    }
};

}
