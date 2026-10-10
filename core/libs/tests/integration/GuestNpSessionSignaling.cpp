#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstdint>

extern "C" {
int APS5_VABI sceNpSessionSignalingInitialize(void* param);
int APS5_VABI sceNpSessionSignalingCreateContext2(const void* param, std::uint32_t* contextId);
std::int32_t APS5_VABI sceNpSessionSignalingGetLocalNetInfo(std::int32_t contextId, void* info);
int APS5_VABI sceNpSessionSignalingRequestPrepare(std::uint32_t contextId, std::uint32_t* requestId);
int APS5_VABI sceNpSessionSignalingTerminate(void);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int invalidArgument = static_cast<int>(0x80553303u);
constexpr int unavailable = static_cast<int>(0x80552D06u);
constexpr std::uint32_t requestSentinel = 0xa5a5a5a5u;

class SignalingContext {
public:
    SignalingContext() {
        unsigned char initializeParam[32] = {};
        RequireEqual(sceNpSessionSignalingInitialize(initializeParam), 0, "initialize");
        unsigned char contextParam[64] = {};
        RequireEqual(sceNpSessionSignalingCreateContext2(contextParam, &id), 0, "context creation");
    }

    ~SignalingContext() {
        sceNpSessionSignalingTerminate();
    }

    SignalingContext(const SignalingContext&) = delete;
    SignalingContext& operator=(const SignalingContext&) = delete;

    std::uint32_t id = 0;
};

const Case prepareNullRequest{"RequestPrepare_NullRequestId_ReturnsInvalidArgument", [] {
    const SignalingContext context;
    RequireEqual(sceNpSessionSignalingRequestPrepare(context.id, nullptr), invalidArgument, "null request id");
}};

const Case prepareWithoutNetwork{"RequestPrepare_NoNetwork_ReturnsUnavailableWithoutWritingId", [] {
    const SignalingContext context;
    std::uint32_t requestId = requestSentinel;
    RequireEqual(sceNpSessionSignalingRequestPrepare(context.id, &requestId), unavailable, "prepare without network");
    RequireEqual(requestId, requestSentinel, "request id after a failed prepare");
}};

const Case localNetInfoNull{"GetLocalNetInfo_NullInfo_ReturnsInvalidArgument", [] {
    const SignalingContext context;
    RequireEqual(sceNpSessionSignalingGetLocalNetInfo(static_cast<std::int32_t>(context.id), nullptr), invalidArgument, "null info");
}};

const Case localNetInfoWithoutNetwork{"GetLocalNetInfo_NoNetwork_ReturnsUnavailableWithoutWritingInfo", [] {
    const SignalingContext context;
    std::array<unsigned char, 16> info{};
    info.fill(0xa5);
    const auto untouched = info;
    RequireEqual(sceNpSessionSignalingGetLocalNetInfo(static_cast<std::int32_t>(context.id), info.data()), unavailable, "local net info without network");
    Require(info == untouched, "failed query modified the info");
}};

const Case terminate{"Terminate_AfterInitialize_Succeeds", [] {
    unsigned char initializeParam[32] = {};
    RequireEqual(sceNpSessionSignalingInitialize(initializeParam), 0, "initialize");
    RequireEqual(sceNpSessionSignalingTerminate(), 0, "terminate");
}};

} // namespace
