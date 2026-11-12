#include "prx/libSceNpManager/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdlib>

static constexpr int ErrorArgument = static_cast<int>(0x80550003);
static constexpr int ErrorAlreadyRegistered = static_cast<int>(0x80550008);
static constexpr int ErrorNotRegistered = static_cast<int>(0x80550009);
static constexpr int ErrorUserNotFound = static_cast<int>(0x80550007);
static constexpr int ErrorInvalidId = static_cast<int>(0x80550015);
static void Require(bool value) { if (!value) std::abort(); }
static void APS5_VABI Callback(int, uint32_t, void*) { std::abort(); }

int main() {
    int userdata = 0;
    auto* callback = reinterpret_cast<void*>(&Callback);
    Require(sceNpUnregisterNpReachabilityStateCallback() == ErrorNotRegistered);
    Require(sceNpRegisterNpReachabilityStateCallback(nullptr, &userdata) == ErrorArgument);
    Require(sceNpRegisterNpReachabilityStateCallback(callback, &userdata) == 0);
    Require(sceNpRegisterNpReachabilityStateCallback(callback, nullptr) == ErrorAlreadyRegistered);
    Require(sceNpRegisterNpReachabilityStateCallback(nullptr, nullptr) == ErrorArgument);
    uint32_t state = 99;
    Require(sceNpGetNpReachabilityState(0, &state) == 0 && state == 0);
    Require(sceNpGetNpReachabilityState(0, nullptr) == ErrorArgument);
    Require(sceNpCheckCallback() == 0);
    Require(sceNpUnregisterNpReachabilityStateCallback() == 0);
    Require(sceNpUnregisterNpReachabilityStateCallback() == ErrorNotRegistered);
    Require(sceNpRegisterNpReachabilityStateCallback(callback, nullptr) == 0);
    Require(sceNpUnregisterNpReachabilityStateCallback() == 0);
    Require(sceNpRegisterPlusEventCallback(nullptr, nullptr) == ErrorArgument);
    Require(sceNpRegisterPremiumEventCallback(nullptr, nullptr) == ErrorArgument);
    Require(sceNpRegisterStateCallback(nullptr, nullptr) == ErrorArgument);
    Require(sceNpUnregisterStateCallback() == ErrorNotRegistered);
    Require(sceNpRegisterStateCallbackA(nullptr, nullptr) == ErrorArgument);
    Require(sceNpUnregisterStateCallbackA(0) == ErrorInvalidId);
    Require(sceNpUnregisterStateCallbackA(99) == ErrorInvalidId);
    int targetUser = 0;
    Require(sceNpGetUserIdByAccountId(0, &targetUser) == ErrorArgument);
    Require(sceNpGetUserIdByAccountId(12345, nullptr) == ErrorArgument);
    bool isPlus = true;
    Require(sceNpIsPlusMember(0, nullptr) == ErrorArgument);
    Require(sceNpIsPlusMember(0, &isPlus) == 0 && !isPlus);
    NpOnlineId onlineId{};
    onlineId.data[0] = 'p';
    uint32_t presence = 99;
    Require(sceNpGetGamePresenceStatus(nullptr, &presence) == ErrorArgument);
    Require(sceNpGetGamePresenceStatus(&onlineId, nullptr) == ErrorArgument);
    Require(sceNpGetGamePresenceStatus(&onlineId, &presence) == 0 && presence == 0);
    Require(sceNpCheckNpAvailability(1, &onlineId, nullptr) == ErrorUserNotFound);
    Require(sceNpCheckNpAvailability(1, nullptr, nullptr) == ErrorArgument);
    std::uint8_t available[3]{0x55, 0x55, 0x55};
    Require(sceNpCheckNpAvailability(0, &onlineId, &available[1]) == ErrorArgument);
    Require(available[1] == 0x55);
    Require(sceNpCheckNpAvailability(1, nullptr, &available[1]) == ErrorArgument);
    Require(available[1] == 0x55);
    Require(sceNpCheckNpAvailability(1, &onlineId, &available[1]) == ErrorUserNotFound);
    Require(available[0] == 0x55 && available[1] == 0 && available[2] == 0x55);
    return 0;
}
