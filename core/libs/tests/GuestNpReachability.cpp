#include "SceTypes.hpp"
#include <cstdlib>

extern "C" {
int APS5_VABI sceNpRegisterNpReachabilityStateCallback(void*, void*);
int APS5_VABI sceNpUnregisterNpReachabilityStateCallback();
int APS5_VABI sceNpGetNpReachabilityState(int, uint32_t*);
int APS5_VABI sceNpCheckCallback();
}

static constexpr int ErrorArgument = static_cast<int>(0x80550003);
static constexpr int ErrorAlreadyRegistered = static_cast<int>(0x80550008);
static constexpr int ErrorNotRegistered = static_cast<int>(0x80550009);
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
}
