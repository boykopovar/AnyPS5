#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <stdexcept>

extern "C" {
std::int32_t APS5_VABI sceBluetoothHidInit();
std::int32_t APS5_VABI sceBluetoothHidRegisterCallback(void* callback, std::int32_t flags, void* param);
std::int32_t APS5_VABI sceBluetoothHidUnregisterCallback();
std::int32_t APS5_VABI sceBluetoothHidRegisterDevice(std::uint16_t vendorId, std::uint16_t productId);
std::int32_t APS5_VABI sceBluetoothHidUnregisterDevice(std::uint16_t vendorId, std::uint16_t productId);
int APS5_VABI sceBluetoothHidGetInputReport();
}

namespace {

using Testing::Case;
using Testing::RequireEqual;
using Testing::RequireThrows;

constexpr std::uint16_t vendorId = 0x44f;
constexpr std::uint16_t productId = 0x1000;

int callbackCalls = 0;

void APS5_VABI Callback() {
    ++callbackCalls;
}

void* CallbackPointer() {
    return reinterpret_cast<void*>(&Callback);
}

class RegisteredCallback {
public:
    RegisteredCallback() {
        RequireEqual(sceBluetoothHidInit(), 0, "initialization");
        RequireEqual(sceBluetoothHidRegisterCallback(CallbackPointer(), 0, param), 0, "callback registration");
    }

    ~RegisteredCallback() {
        sceBluetoothHidUnregisterCallback();
    }

    RegisteredCallback(const RegisteredCallback&) = delete;
    RegisteredCallback& operator=(const RegisteredCallback&) = delete;

    std::uint8_t param[16]{};
};

const Case registerDevice{"RegisterDevice_WithCallback_SucceedsWithoutReportingConnection", [] {
    const RegisteredCallback callback;
    callbackCalls = 0;
    RequireEqual(sceBluetoothHidRegisterDevice(vendorId, productId), 0, "device registration");
    RequireEqual(callbackCalls, 0, "device connection callbacks");
    RequireEqual(sceBluetoothHidUnregisterDevice(vendorId, productId), 0, "device unregistration");
}};

const Case nullCallback{"RegisterCallback_NullCallback_ThrowsInvalidArgument", [] {
    RegisteredCallback callback;
    RequireThrows<std::invalid_argument>([&] { sceBluetoothHidRegisterCallback(nullptr, 0, callback.param); }, "null callback registration");
}};

const Case inputReport{"GetInputReport_NoDevice_ThrowsRuntimeError", [] {
    const RegisteredCallback callback;
    RequireThrows<std::runtime_error>([] { sceBluetoothHidGetInputReport(); }, "input report without a device");
}};

const Case unregisterCallback{"UnregisterCallback_AfterRegister_Succeeds", [] {
    std::uint8_t param[16]{};
    RequireEqual(sceBluetoothHidInit(), 0, "initialization");
    RequireEqual(sceBluetoothHidRegisterCallback(CallbackPointer(), 0, param), 0, "callback registration");
    RequireEqual(sceBluetoothHidUnregisterCallback(), 0, "callback unregistration");
}};

} // namespace
