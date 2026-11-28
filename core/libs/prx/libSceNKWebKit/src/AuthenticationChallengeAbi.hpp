#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct AuthenticationChallenge {
    unsigned char isNull;
    unsigned char reserved0[7];
    unsigned char protectionSpace[72];
    unsigned char proposedCredential[24];
    std::uint32_t previousFailureCount;
    unsigned char reserved1[4];
    unsigned char failureResponse[272];
    unsigned char error[80];
    unsigned char isForMainResource;
    unsigned char isInMainFrame;
    unsigned char reserved2[6];
    WtfAbi::URL requestURL;
};

static_assert(sizeof(AuthenticationChallenge) == 512);
static_assert(alignof(AuthenticationChallenge) == 8);
static_assert(std::is_standard_layout_v<AuthenticationChallenge>);
static_assert(offsetof(AuthenticationChallenge, isNull) == 0);
static_assert(offsetof(AuthenticationChallenge, protectionSpace) == 8);
static_assert(offsetof(AuthenticationChallenge, proposedCredential) == 80);
static_assert(offsetof(AuthenticationChallenge, previousFailureCount) == 104);
static_assert(offsetof(AuthenticationChallenge, failureResponse) == 112);
static_assert(offsetof(AuthenticationChallenge, error) == 384);
static_assert(offsetof(AuthenticationChallenge, isForMainResource) == 464);
static_assert(offsetof(AuthenticationChallenge, isInMainFrame) == 465);
static_assert(offsetof(AuthenticationChallenge, requestURL) == 472);

}
