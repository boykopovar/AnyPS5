#include "prx/libSceMouse/libSceMouse.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int Ok = 0;
constexpr int InvalidArg = static_cast<int>(0x80df0001u);
constexpr int InvalidHandle = static_cast<int>(0x80df0003u);
constexpr int NotInitialized = static_cast<int>(0x80df0005u);
constexpr int NoHandle = static_cast<int>(0x80df0008u);

using GetHandleFn = int (APS5_VABI*)(int, std::int32_t, std::int32_t);
using DeviceInfoFn = int (APS5_VABI*)(std::int32_t, void*);
using HandleValueFn = int (APS5_VABI*)(std::int32_t, std::uint32_t);
using ReadFn = int (APS5_VABI*)(std::int32_t, void*, std::uint32_t, std::int32_t);
using ReadStateFn = int (APS5_VABI*)(std::int32_t, void*, std::uint32_t);
using SetConfigFn = int (APS5_VABI*)(std::int32_t, const void*);
using SetCursorFn = int (APS5_VABI*)(std::int32_t, std::int32_t);
using GetCursorFn = int (APS5_VABI*)(std::int32_t, void*);
using ScaleFn = int (APS5_VABI*)(std::uint16_t*, std::uint16_t);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "Mouse queries: %s\n", message);
    std::abort();
}

template <typename Function, typename Export>
Function As(Export function) {
    return reinterpret_cast<Function>(function);
}

}

int main() {
    const auto getHandle = As<GetHandleFn>(&sceMouseGetHandle);
    const auto getDeviceInfo = As<DeviceInfoFn>(&sceMouseGetDeviceInfo);
    const auto setHandType = As<HandleValueFn>(&sceMouseSetHandType);
    const auto setPointerSpeed = As<HandleValueFn>(&sceMouseSetPointerSpeed);
    const auto extensionRead = As<ReadFn>(&sceMouseExtensionRead);
    const auto extensionReadState = As<ReadStateFn>(&sceMouseExtensionReadState);
    const auto setConfig = As<SetConfigFn>(&sceMouseExtensionSetConfig);
    const auto setCursor = As<SetCursorFn>(&sceMouseExtensionSetCursor);
    const auto getCursor = As<GetCursorFn>(&sceMouseExtensionGetCursor);
    const auto scale = As<ScaleFn>(&sceMouseExtensionScalePosition);

    std::array<std::uint8_t, 0x40> buffer{};
    std::array<std::uint8_t, 0x20> config{};
    std::uint16_t position = 100;

    Require(getHandle(1, 0, 0) == NotInitialized, "GetHandle answered before sceMouseInit");
    Require(getDeviceInfo(0, buffer.data()) == NotInitialized, "GetDeviceInfo answered before sceMouseInit");
    Require(setHandType(1, 0) == NotInitialized && setPointerSpeed(1, 0) == NotInitialized, "SetHandType or SetPointerSpeed answered before sceMouseInit");
    Require(setConfig(1, config.data()) == NotInitialized, "ExtensionSetConfig answered before sceMouseInit");
    Require(setCursor(1, 0) == NotInitialized && getCursor(1, buffer.data()) == NotInitialized, "the cursor calls answered before sceMouseInit");
    Require(scale(&position, 1920) == NotInitialized && position == 100, "ExtensionScalePosition scaled before sceMouseInit");

    Require(sceMouseInit() == Ok, "sceMouseInit failed");
    Require(getHandle(1, 0, 0) == NoHandle, "GetHandle found a handle before sceMouseOpen");
    const int handle = sceMouseOpen(1, 0, 0, nullptr);
    Require(handle > 0, "sceMouseOpen failed");
    Require(getHandle(1, 0, 0) == handle && getHandle(1, 0, 1) == handle, "GetHandle didn't return the open handle for its user and type");
    Require(getHandle(2, 0, 0) == NoHandle && getHandle(1, 1, 0) == NoHandle, "GetHandle matched another user or type");

    Require(scale(nullptr, 1920) == InvalidArg, "ExtensionScalePosition took a null position");
    Require(scale(&position, 0) == InvalidArg && position == 100, "ExtensionScalePosition took a scale of 0");
    const std::array<std::array<std::uint16_t, 3>, 9> rows{{{65535, 1920, 1919}, {32768, 1920, 960}, {0, 1920, 0}, {65535, 1, 0}, {1, 65535, 1}, {40000, 1080, 659}, {1, 2, 1}, {32767, 2, 1}, {100, 1920, 3}}};
    for (const auto& [input, size, expected] : rows) {
        position = input;
        Require(scale(&position, size) == Ok && position == expected, "ExtensionScalePosition didn't round position * (scale - 1) / 65535 up");
    }

    Require(setConfig(handle, nullptr) == InvalidArg, "ExtensionSetConfig took a null config");
    config[5] = 1;
    Require(setConfig(handle, config.data()) == InvalidArg, "ExtensionSetConfig took a non-zero byte 5");
    config[5] = 0;
    config[0x1f] = 1;
    Require(setConfig(handle, config.data()) == InvalidArg, "ExtensionSetConfig took a non-zero byte 0x1f");
    config[0x1f] = 0;
    config[0] = 0x12;
    config[4] = 1;
    Require(setConfig(handle + 1, config.data()) == InvalidHandle, "ExtensionSetConfig took an unopened handle");
    Require(setConfig(handle, config.data()) == Ok, "ExtensionSetConfig refused a valid config");

    Require(setHandType(handle, 2) == InvalidArg, "SetHandType took a type above 1");
    Require(setPointerSpeed(handle, 9) == InvalidArg, "SetPointerSpeed took a speed above 8");
    Require(getDeviceInfo(0, nullptr) == InvalidArg, "GetDeviceInfo took a null output");

    Require(extensionRead(handle, buffer.data(), 0x20, 1) == InvalidArg, "ExtensionRead took a record size other than 0x30");
    Require(extensionRead(handle + 1, buffer.data(), 0x30, 1) == InvalidHandle, "ExtensionRead took an unopened handle");
    Require(extensionRead(handle, nullptr, 0x30, 1) == InvalidArg && extensionRead(handle, buffer.data(), 0x30, 0) == InvalidArg, "ExtensionRead took a null buffer or a count of 0");
    Require(extensionReadState(handle, buffer.data(), 0x30) == InvalidArg && extensionReadState(handle, nullptr, 0x40) == InvalidArg, "ExtensionReadState took a size other than 0x40 or a null state");
    std::memset(buffer.data(), 0xaa, buffer.size());
    Require(extensionReadState(handle + 1, buffer.data(), 0x40) == InvalidHandle, "ExtensionReadState took an unopened handle");
    for (const auto byte : buffer) Require(byte == 0, "ExtensionReadState didn't clear the state before the handle check");

    Require(setCursor(handle + 1, 0) == InvalidHandle, "ExtensionSetCursor took an unopened handle");
    Require(getCursor(handle, nullptr) == InvalidArg && getCursor(handle + 1, nullptr) == InvalidArg, "ExtensionGetCursor didn't check its output before the handle");
    Require(getCursor(handle + 1, buffer.data()) == InvalidHandle, "ExtensionGetCursor took an unopened handle");

    Require(sceMouseClose(handle) == Ok, "sceMouseClose failed");
    Require(getHandle(1, 0, 0) == NoHandle, "GetHandle returned a closed handle");
    std::puts("Mouse query tests passed");
    return 0;
}
