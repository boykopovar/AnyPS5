#include "SceTypes.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string_view>

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
int APS5_VABI sceRudpActivate();
int APS5_VABI sceRudpGetStatus(void*, std::size_t);
int APS5_VABI sceRudpTerminate();
}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    constexpr int invalidValue = static_cast<int>(0x804311FE);
    constexpr int network = static_cast<int>(0x80431063);
    Require(sceHttpSetCookieEnabled(1, 0) == 0);
    Require(sceHttpSendRequest(1, nullptr, 0) == network);
    bool cookieUnsupported = false;
    try {
        sceHttpSetCookieEnabled(1, 1);
    } catch (const std::runtime_error& error) {
        cookieUnsupported = std::string_view(error.what()) == "sceHttpSetCookieEnabled not implemented";
    }
    Require(cookieUnsupported);
    for (int enabled : {-1, 2, 0x100}) {
        Require(sceHttpSetCookieEnabled(1, enabled) == invalidValue);
    }

    constexpr int parameter = static_cast<int>(0x817D0002);
    constexpr int noEntitlement = static_cast<int>(0x817D0007);
    static_assert(sizeof(NpEntitlementAccessEntitlementKey) == 16);
    NpUnifiedEntitlementLabel label{};
    std::memcpy(label.data, "unowned-addon", 14);
    struct KeyBuffer {
        std::uint64_t before;
        NpEntitlementAccessEntitlementKey key;
        std::uint64_t after;
    } output;
    std::memset(&output, 0xa5, sizeof(output));
    std::array<unsigned char, sizeof(output)> original{};
    std::memcpy(original.data(), &output, sizeof(output));
    Require(sceNpEntitlementAccessGetEntitlementKey(0, nullptr, &output.key) == parameter);
    Require(sceNpEntitlementAccessGetEntitlementKey(0, &label, nullptr) == parameter);
    Require(sceNpEntitlementAccessGetEntitlementKey(0, nullptr, nullptr) == parameter);
    for (std::uint32_t serviceLabel : {0u, 1u, 0xffffffffu}) {
        Require(sceNpEntitlementAccessGetEntitlementKey(serviceLabel, &label, &output.key) == noEntitlement);
        Require(std::memcmp(&output, original.data(), sizeof(output)) == 0);
    }

    constexpr int signedOut = static_cast<int>(0x80550006);
    Require(sceNpEntitlementAccessRequestUnifiedEntitlementInfoList() == signedOut);
    Require(sceNpEntitlementAccessPollUnifiedEntitlementInfoList() == signedOut);
    Require(sceNpEntitlementAccessRequestServiceEntitlementInfoList() == signedOut);
    Require(sceNpEntitlementAccessPollServiceEntitlementInfoList() == signedOut);

    Require(sceRudpActivate() == 0);
    std::array<unsigned char, 248> status;
    status.fill(0x5a);
    const auto originalStatus = status;
    constexpr int rudpNotInitialized = static_cast<int>(0x80770001);
    Require(sceRudpGetStatus(status.data(), status.size()) == rudpNotInitialized);
    Require(status == originalStatus);
    Require(sceRudpInit_nid_postfix(nullptr, 0) == 0);
    Require(sceRudpActivate() == 0);
    Require(sceRudpGetStatus(status.data(), status.size()) == 0);
    for (unsigned char byte : status) Require(byte == 0);
    Require(sceRudpGetStatus(nullptr, 0) == 0);
    Require(sceRudpTerminate() == 0);
    return 0;
}
