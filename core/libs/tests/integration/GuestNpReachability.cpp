#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <atomic>
#include <cstdint>

extern "C" {
int APS5_VABI sceNpRegisterNpReachabilityStateCallback(void*, void*);
int APS5_VABI sceNpUnregisterNpReachabilityStateCallback();
int APS5_VABI sceNpGetNpReachabilityState(int, uint32_t*);
int APS5_VABI sceNpCheckCallback();
int APS5_VABI sceNpCheckNpAvailability(int, const NpOnlineId*);
}

namespace {

using Testing::Case;
using Testing::RequireEqual;

constexpr int errorArgument = static_cast<int>(0x80550003);
constexpr int errorAlreadyRegistered = static_cast<int>(0x80550008);
constexpr int errorNotRegistered = static_cast<int>(0x80550009);
constexpr int errorUserNotFound = static_cast<int>(0x80550007);

std::atomic<int> callbackInvocations{0};

void APS5_VABI Callback(int, uint32_t, void*) {
    ++callbackInvocations;
}

void* CallbackPointer() {
    return reinterpret_cast<void*>(&Callback);
}

class RegisteredCallback {
public:
    explicit RegisteredCallback(void* userdata) {
        RequireEqual(sceNpRegisterNpReachabilityStateCallback(CallbackPointer(), userdata), 0, "register the callback");
    }

    ~RegisteredCallback() {
        sceNpUnregisterNpReachabilityStateCallback();
    }

    RegisteredCallback(const RegisteredCallback&) = delete;
    RegisteredCallback& operator=(const RegisteredCallback&) = delete;
};

const Case unregisterWithoutCallback{"UnregisterCallback_NothingRegistered_ReturnsNotRegistered", [] {
    RequireEqual(sceNpUnregisterNpReachabilityStateCallback(), errorNotRegistered, "unregister without a callback");
}};

const Case registerNullCallback{"RegisterCallback_NullCallback_ReturnsArgumentError", [] {
    int userdata = 0;
    RequireEqual(sceNpRegisterNpReachabilityStateCallback(nullptr, &userdata), errorArgument, "null callback");
    RequireEqual(sceNpUnregisterNpReachabilityStateCallback(), errorNotRegistered, "nothing was registered");
}};

const Case registerTwice{"RegisterCallback_AlreadyRegistered_ReturnsAlreadyRegistered", [] {
    int userdata = 0;
    const RegisteredCallback registered(&userdata);
    RequireEqual(sceNpRegisterNpReachabilityStateCallback(CallbackPointer(), nullptr), errorAlreadyRegistered, "second registration");
}};

const Case registerNullWhileRegistered{"RegisterCallback_NullCallbackWhileRegistered_ReturnsArgumentError", [] {
    int userdata = 0;
    const RegisteredCallback registered(&userdata);
    RequireEqual(sceNpRegisterNpReachabilityStateCallback(nullptr, nullptr), errorArgument, "null callback while registered");
}};

const Case reachabilityState{"GetReachabilityState_DefaultUser_ReportsUnavailable", [] {
    int userdata = 0;
    const RegisteredCallback registered(&userdata);
    uint32_t state = 99;
    RequireEqual(sceNpGetNpReachabilityState(0, &state), 0, "get reachability state");
    RequireEqual(state, 0u, "reachability state");
}};

const Case checkCallback{"CheckCallback_WhileRegistered_SucceedsWithoutInvokingCallback", [] {
    int userdata = 0;
    const RegisteredCallback registered(&userdata);
    callbackInvocations = 0;
    RequireEqual(sceNpCheckCallback(), 0, "check callback");
    RequireEqual(callbackInvocations.load(), 0, "callback invocations");
}};

const Case unregisterOnce{"UnregisterCallback_AfterRegister_SucceedsOnlyOnce", [] {
    int userdata = 0;
    RequireEqual(sceNpRegisterNpReachabilityStateCallback(CallbackPointer(), &userdata), 0, "register");
    RequireEqual(sceNpUnregisterNpReachabilityStateCallback(), 0, "first unregister");
    RequireEqual(sceNpUnregisterNpReachabilityStateCallback(), errorNotRegistered, "second unregister");
}};

const Case registerNullUserdata{"RegisterCallback_NullUserdata_Succeeds", [] {
    RequireEqual(sceNpRegisterNpReachabilityStateCallback(CallbackPointer(), nullptr), 0, "register with null userdata");
    RequireEqual(sceNpUnregisterNpReachabilityStateCallback(), 0, "unregister");
}};

const Case availabilityUnknownUser{"CheckNpAvailability_UnknownUser_ReturnsUserNotFound", [] {
    NpOnlineId onlineId{};
    onlineId.data[0] = 'p';
    RequireEqual(sceNpCheckNpAvailability(1, &onlineId), errorUserNotFound, "availability of an unknown user");
}};

const Case availabilityNullId{"CheckNpAvailability_NullOnlineId_ReturnsArgumentError", [] {
    RequireEqual(sceNpCheckNpAvailability(1, nullptr), errorArgument, "availability with a null online id");
}};

} // namespace
