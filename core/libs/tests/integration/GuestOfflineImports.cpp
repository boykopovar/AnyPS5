#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sceHttpSetCookieEnabled(int, int);
int APS5_VABI sceHttpSendRequest(int, const void*, std::size_t);
int APS5_VABI sceNpEntitlementAccessGetEntitlementKey(
    std::uint32_t, const NpUnifiedEntitlementLabel*, NpEntitlementAccessEntitlementKey*);
int APS5_VABI sceNpEntitlementAccessRequestUnifiedEntitlementInfoList();
int APS5_VABI sceNpEntitlementAccessPollUnifiedEntitlementInfoList();
int APS5_VABI sceNpEntitlementAccessRequestServiceEntitlementInfoList();
int APS5_VABI sceNpEntitlementAccessPollServiceEntitlementInfoList();
int APS5_VABI sceRudpInit_nid_postfix(void*, int);
int APS5_VABI sceRudpGetStatus(void*, std::size_t);
int APS5_VABI sceRudpTerminate();
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int httpInvalidValue = static_cast<int>(0x804311FE);
constexpr int httpNetwork = static_cast<int>(0x80431063);
constexpr int entitlementParameter = static_cast<int>(0x817D0002);
constexpr int entitlementNoEntitlement = static_cast<int>(0x817D0007);
constexpr int npSignedOut = static_cast<int>(0x80550006);
constexpr int rudpNotInitialized = static_cast<int>(0x80770001);

static_assert(sizeof(NpEntitlementAccessEntitlementKey) == 16);

struct KeyBuffer {
    std::uint64_t before;
    NpEntitlementAccessEntitlementKey key;
    std::uint64_t after;
};

class RudpSession {
public:
    RudpSession() { RequireEqual(sceRudpInit_nid_postfix(nullptr, 0), 0, "sceRudpInit"); }
    ~RudpSession() { sceRudpTerminate(); }
    RudpSession(const RudpSession&) = delete;
    RudpSession& operator=(const RudpSession&) = delete;
};

NpUnifiedEntitlementLabel UnownedLabel() {
    NpUnifiedEntitlementLabel label{};
    std::memcpy(label.data, "unowned-addon", 14);
    return label;
}

const Case cookiesDisabled{"HttpSetCookieEnabled_Disable_Succeeds", [] {
    RequireEqual(sceHttpSetCookieEnabled(1, 0), 0, "disable cookies");
}};

const Case sendRequest{"HttpSendRequest_Offline_FailsWithNetworkError", [] {
    RequireEqual(sceHttpSendRequest(1, nullptr, 0), httpNetwork, "send request");
}};

const Case cookiesEnabled{"HttpSetCookieEnabled_Enable_ThrowsNotImplemented", [] {
    bool caught = false;
    std::string message;
    try {
        sceHttpSetCookieEnabled(1, 1);
    } catch (const std::runtime_error& error) {
        caught = true;
        message = error.what();
    }
    Require(caught, "enable cookies throws std::runtime_error");
    RequireEqual(message, std::string("sceHttpSetCookieEnabled not implemented"), "what()");
}};

const Case cookiesInvalid{"HttpSetCookieEnabled_InvalidFlag_FailsWithInvalidValue", [] {
    for (const int enabled : {-1, 2, 0x100}) {
        RequireEqual(sceHttpSetCookieEnabled(1, enabled), httpInvalidValue, "enabled " + std::to_string(enabled));
    }
}};

const Case keyNullArguments{"EntitlementKey_NullLabelOrKey_FailsWithParameterError", [] {
    const auto label = UnownedLabel();
    KeyBuffer output{};
    RequireEqual(sceNpEntitlementAccessGetEntitlementKey(0, nullptr, &output.key), entitlementParameter, "null label");
    RequireEqual(sceNpEntitlementAccessGetEntitlementKey(0, &label, nullptr), entitlementParameter, "null key");
    RequireEqual(sceNpEntitlementAccessGetEntitlementKey(0, nullptr, nullptr), entitlementParameter, "null label and key");
}};

const Case keyUnowned{"EntitlementKey_UnownedLabel_FailsWithNoEntitlementAndKeepsOutput", [] {
    const auto label = UnownedLabel();
    KeyBuffer output{};
    std::memset(&output, 0xa5, sizeof(output));
    std::array<unsigned char, sizeof(output)> original{};
    std::memcpy(original.data(), &output, sizeof(output));
    for (const std::uint32_t serviceLabel : {0u, 1u, 0xffffffffu}) {
        const std::string input = "service label " + std::to_string(serviceLabel);
        RequireEqual(sceNpEntitlementAccessGetEntitlementKey(serviceLabel, &label, &output.key), entitlementNoEntitlement, input);
        Require(std::memcmp(&output, original.data(), sizeof(output)) == 0, input + " output untouched");
    }
}};

const Case entitlementLists{"EntitlementInfoLists_RequestAndPoll_FailWithSignedOut", [] {
    RequireEqual(sceNpEntitlementAccessRequestUnifiedEntitlementInfoList(), npSignedOut, "request unified list");
    RequireEqual(sceNpEntitlementAccessPollUnifiedEntitlementInfoList(), npSignedOut, "poll unified list");
    RequireEqual(sceNpEntitlementAccessRequestServiceEntitlementInfoList(), npSignedOut, "request service list");
    RequireEqual(sceNpEntitlementAccessPollServiceEntitlementInfoList(), npSignedOut, "poll service list");
}};

const Case rudpUninitialized{"RudpGetStatus_BeforeInit_FailsAndKeepsBuffer", [] {
    std::array<unsigned char, 248> status;
    status.fill(0x5a);
    const auto originalStatus = status;
    RequireEqual(sceRudpGetStatus(status.data(), status.size()), rudpNotInitialized, "get status");
    Require(status == originalStatus, "status buffer untouched");
}};

const Case rudpStatus{"RudpGetStatus_AfterInit_ZeroFillsBuffer", [] {
    const RudpSession session;
    std::array<unsigned char, 248> status;
    status.fill(0x5a);
    RequireEqual(sceRudpGetStatus(status.data(), status.size()), 0, "get status");
    for (std::size_t index = 0; index < status.size(); ++index) {
        RequireEqual(status[index], static_cast<unsigned char>(0), "status byte " + std::to_string(index));
    }
}};

const Case rudpNullStatus{"RudpGetStatus_NullBufferAfterInit_Succeeds", [] {
    const RudpSession session;
    RequireEqual(sceRudpGetStatus(nullptr, 0), 0, "get status");
}};

const Case rudpTerminate{"RudpTerminate_AfterInit_Succeeds", [] {
    RequireEqual(sceRudpInit_nid_postfix(nullptr, 0), 0, "sceRudpInit");
    RequireEqual(sceRudpTerminate(), 0, "sceRudpTerminate");
}};

} // namespace
