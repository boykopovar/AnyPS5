#include "prx/libSceBluetoothHid/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cerrno>
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
    errno = 71;
    Require(sceBluetoothHidRegisterCallback(nullptr, nullptr, reinterpret_cast<const BluetoothHidParam*>(0x10)) == notInitialized, "register callback unmapped before initialization");
    Require(errno == 71, "errno preserved before initialization");
    Require(sceBluetoothHidRegisterCallback(&Callback, nullptr, nullptr) == notInitialized, "non-null callback before initialization");
    Require(sceBluetoothHidRegisterDevice(1, 2) == notInitialized, "registration before initialization");
    Require(sceBluetoothHidUnregisterDevice(1, 2) == notInitialized, "unregistration before initialization");
    Require(sceBluetoothHidUnregisterCallback() == notInitialized, "callback removal before initialization");
    Require(sceBluetoothHidGetDeviceName(1, nullptr) == notInitialized, "get device name before initialization");
    Require(sceBluetoothHidGetInputReport(0, 0) == notInitialized, "get input report before initialization");
    Require(sceBluetoothHidSetOutputReport(0, 0, nullptr, 0) == notInitialized, "set output report before initialization");
    Require(sceBluetoothHidDisconnectDevice(0) == notInitialized, "disconnect device before initialization");
    Require(sceBluetoothHidGetDeviceInfo(0, nullptr) == notInitialized, "get device info before initialization");
    Require(sceBluetoothHidGetFeatureReport(0, 0) == notInitialized, "get feature report before initialization");
    Require(sceBluetoothHidGetReportDescriptor(0, nullptr, 0) == notInitialized, "get report descriptor before initialization");
    Require(sceBluetoothHidInterruptOutput(0, 0, nullptr, 0) == notInitialized, "interrupt output before initialization");
    Require(sceBluetoothHidSetFeatureReport(0, 0, nullptr, 0) == notInitialized, "set feature report before initialization");
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
    errno = 71;
    Require(sceBluetoothHidRegisterCallback(&Callback, nullptr, reinterpret_cast<const BluetoothHidParam*>(0x10)) == invalidArgument, "unmapped param validation");
    Require(errno == 71, "errno preserved for unmapped param");
    BluetoothHidParam badThreadParam{};
    badThreadParam.threadParam = reinterpret_cast<const BluetoothHidThreadParam*>(0x10);
    errno = 71;
    Require(sceBluetoothHidRegisterCallback(&Callback, nullptr, &badThreadParam) == invalidArgument, "unmapped threadParam validation");
    Require(errno == 71, "errno preserved for unmapped threadParam");
    param.reserved = 1;
    Require(sceBluetoothHidRegisterCallback(&Callback, nullptr, &param) == invalidArgument, "reserved field validation");
    param.reserved = 0;
    BluetoothHidThreadParam threadParam{};
    threadParam.reserved = 1;
    param.threadParam = &threadParam;
    Require(sceBluetoothHidRegisterCallback(&Callback, nullptr, &param) == invalidArgument, "thread reserved field validation");
    Require(sceBluetoothHidRegisterCallback(&Callback, nullptr, nullptr) == 0, "optional parameters rejected");
    bool threw = false;
    Require(sceBluetoothHidGetDeviceName(1, nullptr) == invalidArgument, "null device name output");
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
    errno = 71;
    Require(sceBluetoothHidParamInitialize(reinterpret_cast<BluetoothHidParam*>(0x10), &thread) == invalidArgument, "unwritable parameter block was not refused");
    Require(errno == 71, "errno modified by invalid pointer check");
    Require(sceBluetoothHidParamInitialize(nullptr, nullptr) == invalidArgument, "null parameter and null thread parameter were not refused");
    errno = 71;
    Require(sceBluetoothHidParamInitialize(reinterpret_cast<BluetoothHidParam*>(0x10), &thread) == invalidArgument, "unmapped param block was not refused");
    Require(errno == 71, "errno preserved on unmapped param block");
    BluetoothHidThreadParam built;
    std::memset(&built, 0x5a, sizeof(built));
    Require(sceBluetoothHidThreadParamInitialize(&built, 700, 0x3f) == 0, "thread parameter initialization did not return 0");
    unsigned char threadBytes[sizeof(BluetoothHidThreadParam)];
    std::memcpy(threadBytes, &built, sizeof(threadBytes));
    constexpr unsigned char expected[20] = {0xbc, 0x02, 0x00, 0x00, 0x5a, 0x5a, 0x5a, 0x5a, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    Require(std::memcmp(threadBytes, expected, 4) == 0, "the priority was not stored as 32 bits at offset 0");
    Require(std::memcmp(threadBytes + 4, expected + 4, 4) == 0, "bytes 4 to 7 of the thread parameter block changed");
    Require(std::memcmp(threadBytes + 8, expected + 8, 8) == 0, "the affinity mask was not stored as 64 bits at offset 8");
    Require(std::memcmp(threadBytes + 16, expected + 16, 4) == 0, "the word at offset 16 was not cleared");
    Require(sceBluetoothHidThreadParamInitialize(nullptr, 700, 0x3f) == invalidArgument, "a null thread parameter block was not refused");
    Require(sceBluetoothHidThreadParamInitialize(nullptr, 0, 0) == invalidArgument, "a null thread parameter block with zero values was not refused");
    errno = 71;
    Require(sceBluetoothHidThreadParamInitialize(reinterpret_cast<BluetoothHidThreadParam*>(0x10), 700, 0x3f) == invalidArgument, "unmapped thread parameter block was not refused");
    Require(errno == 71, "errno preserved on unmapped thread parameter block");
    std::puts("BluetoothHid tests passed");
    return 0;
}
