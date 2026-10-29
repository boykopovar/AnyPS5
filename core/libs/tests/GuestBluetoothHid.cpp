#include "prx/libSceBluetoothHid/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

int callbackCalls = 0;

void APS5_VABI Callback(const BluetoothHidEvent*, std::int32_t, void*) {
    ++callbackCalls;
}

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "BluetoothHid: %s\n", message);
        std::abort();
    }
}

}

int main() {
    constexpr auto notInitialized = static_cast<std::int32_t>(0x81540004u);
    constexpr auto invalidArgument = static_cast<std::int32_t>(0x81540001u);
    constexpr auto noCallback = static_cast<std::int32_t>(0x81540006u);
    Require(sceBluetoothHidRegisterCallback(nullptr, nullptr, nullptr) == notInitialized, "initialization check order");
    Require(sceBluetoothHidRegisterCallback(&Callback, nullptr, nullptr) == notInitialized, "non-null callback before initialization");
    Require(sceBluetoothHidRegisterDevice(1, 2) == notInitialized, "registration before initialization");
    Require(sceBluetoothHidUnregisterDevice(1, 2) == notInitialized, "unregistration before initialization");
    Require(sceBluetoothHidUnregisterCallback() == notInitialized, "callback removal before initialization");
    Require(sceBluetoothHidInit() == 0, "initialization failed");
    Require(sceBluetoothHidInit() == 0, "repeated initialization failed");
    Require(sceBluetoothHidRegisterDevice(1, 2) == noCallback, "registration without callback");
    Require(sceBluetoothHidUnregisterDevice(1, 2) == 0, "device unregistration does not need a registered callback");
    Require(sceBluetoothHidUnregisterCallback() == noCallback, "removing absent callback");
    BluetoothHidParam param{};
    Require(sceBluetoothHidRegisterCallback(&Callback, &callbackCalls, &param) == 0, "callback registration failed");
    Require(sceBluetoothHidRegisterDevice(0x44f, 0x1000) == 0, "device registration failed");
    Require(callbackCalls == 0, "a device connection was reported");
    Require(sceBluetoothHidRegisterCallback(nullptr, nullptr, &param) == invalidArgument, "null callback validation");
    param.reserved = 1;
    Require(sceBluetoothHidRegisterCallback(&Callback, nullptr, &param) == invalidArgument, "reserved field validation");
    param.reserved = 0;
    BluetoothHidThreadParam threadParam{};
    threadParam.reserved = 1;
    param.threadParam = &threadParam;
    Require(sceBluetoothHidRegisterCallback(&Callback, nullptr, &param) == invalidArgument, "thread reserved field validation");
    Require(sceBluetoothHidRegisterCallback(&Callback, nullptr, nullptr) == 0, "optional parameters rejected");
    bool threw = false;
    threw = false;
    try {
        sceBluetoothHidGetInputReport(0, 0);

    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "an input report without a device did not throw");
    Require(sceBluetoothHidDebugGetVersion() == 0x1e, "debug get version did not return the firmware version");
    threw = false;
    try {
        sceBluetoothHidGetDeviceInfo(0, nullptr);

    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "get device info did not throw");
    threw = false;
    try {
        sceBluetoothHidDisconnectDevice(0);

    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "disconnect device did not throw");
    threw = false;
    try {
        sceBluetoothHidGetFeatureReport(0, 0);

    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "get feature report did not throw");
    threw = false;
    try {
        sceBluetoothHidSetFeatureReport(0, 0, nullptr, 0);

    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "set feature report did not throw");
    Require(sceBluetoothHidUnregisterDevice(0x44f, 0x1000) == 0, "device unregistration failed");
    Require(sceBluetoothHidUnregisterCallback() == 0, "callback unregistration failed");
    Require(sceBluetoothHidUnregisterCallback() == noCallback, "duplicate callback unregistration");
    Require(sceBluetoothHidRegisterDevice(0x44f, 0x1000) == noCallback, "device registration after callback unregistration");
    BluetoothHidThreadParam thread{};
    BluetoothHidParam filled;
    std::memset(&filled, 0xa5, sizeof(filled));
    Require(sceBluetoothHidParamInitialize(&filled, &thread) == 0, "parameter initialization did not return 0");
    unsigned char bytes[sizeof(BluetoothHidParam)];
    std::memcpy(bytes, &filled, sizeof(bytes));
    const BluetoothHidThreadParam* stored = nullptr;
    std::memcpy(&stored, bytes, sizeof(stored));
    std::uint32_t cleared = 1;
    std::memcpy(&cleared, bytes + 8, sizeof(cleared));
    Require(stored == &thread, "the thread parameter was not stored at offset 0");
    Require(cleared == 0, "the word at offset 8 was not cleared");
    Require(bytes[12] == 0xa5 && bytes[13] == 0xa5 && bytes[14] == 0xa5 && bytes[15] == 0xa5, "bytes 12 to 15 of the parameter block changed");
    std::memset(&filled, 0x5a, sizeof(filled));
    Require(sceBluetoothHidParamInitialize(&filled, nullptr) == 0 && filled.threadParam == nullptr && filled.reserved == 0 && filled.padding == 0x5a5a5a5au, "a null thread parameter was not stored as given");
    Require(sceBluetoothHidParamInitialize(nullptr, &thread) == invalidArgument, "a null parameter block was not refused");
    Require(sceBluetoothHidParamInitialize(nullptr, nullptr) == invalidArgument, "null parameter and null thread parameter were not refused");
    std::puts("BluetoothHid tests passed");
    return 0;
}
