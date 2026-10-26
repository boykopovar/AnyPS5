#include "prx/libSceNpManager/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdlib>

static constexpr int ErrorArgument = static_cast<int>(0x80550003);
static constexpr int ErrorAlreadyRegistered = static_cast<int>(0x80550008);
static constexpr int ErrorNotRegistered = static_cast<int>(0x80550009);
static constexpr int ErrorUserNotFound = static_cast<int>(0x80550007);
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
    Require(sceNpCheckCallback() == 0);
    Require(sceNpUnregisterNpReachabilityStateCallback() == 0);
    Require(sceNpUnregisterNpReachabilityStateCallback() == ErrorNotRegistered);
    Require(sceNpRegisterNpReachabilityStateCallback(callback, nullptr) == 0);
    Require(sceNpUnregisterNpReachabilityStateCallback() == 0);
    NpOnlineId onlineId{};
    onlineId.data[0] = 'p';
    Require(sceNpCheckNpAvailability(1, &onlineId, nullptr) == ErrorUserNotFound);
    Require(sceNpCheckNpAvailability(1, nullptr, nullptr) == ErrorArgument);
    std::uint8_t available[3]{0x55, 0x55, 0x55};
    Require(sceNpCheckNpAvailability(0, &onlineId, &available[1]) == ErrorArgument);
    Require(available[1] == 0x55);
    Require(sceNpCheckNpAvailability(1, nullptr, &available[1]) == ErrorArgument);
    Require(available[1] == 0x55);
    Require(sceNpCheckNpAvailability(1, &onlineId, &available[1]) == ErrorUserNotFound);
    Require(available[0] == 0x55 && available[1] == 0 && available[2] == 0x55);

}
