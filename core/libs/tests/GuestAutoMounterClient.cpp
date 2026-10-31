#include "prx/libSceAutoMounterClient/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" [[noreturn]] void NotImplemented_nid_no_patch(const char* funcName) {
    throw std::runtime_error(std::string(funcName ? funcName : "unimplemented") + " not implemented");
}

namespace {

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message.c_str());
        std::abort();
    }
}

template <typename TFunc>
void checkThrows(TFunc func, const std::string& message) {
    bool threw = false;
    try {
        func();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, message);
}

bool isBufferUntouched(const void* buffer, std::size_t size, std::uint8_t sentinel) {
    const auto* bytes = static_cast<const std::uint8_t*>(buffer);
    for (std::size_t i = 0; i < size; ++i) {
        if (bytes[i] != sentinel) {
            return false;
        }
    }
    return true;
}

}

int main() {
    checkThrows([] {
        sceAutoMounterClientInit(nullptr);
    }, "sceAutoMounterClientInit did not throw std::runtime_error");

    checkThrows([] {
        sceAutoMounterClientTerm();
    }, "sceAutoMounterClientTerm did not throw std::runtime_error");

    checkThrows([] {
        sceAutoMounterClientRegisterCallback();
    }, "sceAutoMounterClientRegisterCallback did not throw std::runtime_error");

    checkThrows([] {
        sceAutoMounterClientUnregisterCallback();
    }, "sceAutoMounterClientUnregisterCallback did not throw std::runtime_error");

    AutoMounterUsbDeviceInfo info;
    std::memset(&info, 0xAA, sizeof(info));

    checkThrows([&] {
        sceAutoMounterClientGetUsbDeviceInfo("/dev/da0", &info);
    }, "sceAutoMounterClientGetUsbDeviceInfo did not throw std::runtime_error");
    check(isBufferUntouched(&info, sizeof(info), 0xAA), "AutoMounterUsbDeviceInfo buffer was modified");

    checkThrows([&] {
        sceAutoMounterClientGetUsbDeviceInfo(nullptr, &info);
    }, "sceAutoMounterClientGetUsbDeviceInfo did not throw std::runtime_error");
    check(isBufferUntouched(&info, sizeof(info), 0xAA), "AutoMounterUsbDeviceInfo buffer was modified");

    checkThrows([] {
        sceAutoMounterClientGetUsbDeviceInfo(nullptr, nullptr);
    }, "sceAutoMounterClientGetUsbDeviceInfo did not throw std::runtime_error");

    char deviceNames[8][64];
    std::memset(deviceNames, 0xAA, sizeof(deviceNames));
    std::int32_t count = static_cast<std::int32_t>(0xAAAAAAAA);

    checkThrows([&] {
        sceAutoMounterClientGetUsbDeviceList(deviceNames, &count);
    }, "sceAutoMounterClientGetUsbDeviceList did not throw std::runtime_error");
    check(isBufferUntouched(deviceNames, sizeof(deviceNames), 0xAA), "device list buffer was modified");
    check(count == static_cast<std::int32_t>(0xAAAAAAAA), "device count was modified");

    checkThrows([&] {
        sceAutoMounterClientGetUsbDeviceList(nullptr, &count);
    }, "sceAutoMounterClientGetUsbDeviceList did not throw std::runtime_error");
    check(count == static_cast<std::int32_t>(0xAAAAAAAA), "device count was modified");

    checkThrows([&] {
        sceAutoMounterClientGetUsbDeviceList(deviceNames, nullptr);
    }, "sceAutoMounterClientGetUsbDeviceList did not throw std::runtime_error");
    check(isBufferUntouched(deviceNames, sizeof(deviceNames), 0xAA), "device list buffer was modified");

    checkThrows([] {
        sceAutoMounterClientGetUsbDeviceList(nullptr, nullptr);
    }, "sceAutoMounterClientGetUsbDeviceList did not throw std::runtime_error");

    return 0;
}
