#include "prx/libSceMbus/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>

static_assert(std::is_same_v<decltype(&sceMbusEventCallbackFuncsInit_), void (APS5_VABI *)(MbusEventCallbacks*, std::uint32_t)>);

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "DeviceService: %s\n", message);
        std::abort();
    }
}

}

int main() {
    static_assert(sizeof(MbusEventCallbacks) == 16);
    alignas(MbusEventCallbacks) std::array<std::uint8_t, 24> callbackBytes{};
    callbackBytes.fill(0xa5);
    sceMbusEventCallbackFuncsInit_(reinterpret_cast<MbusEventCallbacks*>(callbackBytes.data()), 8);
    for (std::size_t i = 0; i < callbackBytes.size(); ++i) {
        Require(callbackBytes[i] == (i < 8 ? 0 : 0xa5), "partial callback initialization changed an unexpected byte");
    }
    sceMbusEventCallbackFuncsInit_(reinterpret_cast<MbusEventCallbacks*>(callbackBytes.data()), sizeof(MbusEventCallbacks));
    for (std::size_t i = 0; i < callbackBytes.size(); ++i) {
        Require(callbackBytes[i] == (i < sizeof(MbusEventCallbacks) ? 0 : 0xa5), "full callback initialization changed an unexpected byte");
    }
    callbackBytes.fill(0x5a);
    sceMbusEventCallbackFuncsInit_(reinterpret_cast<MbusEventCallbacks*>(callbackBytes.data()), 0);
    for (const auto byte : callbackBytes) Require(byte == 0x5a, "zero-size callback initialization changed memory");

    DeviceServiceInitParam param{};
    constexpr auto invalidArgument = static_cast<std::int32_t>(0x809B0080u);
    Require(sceDeviceServiceInitialize(3, &param) == invalidArgument, "invalid parameter size accepted");
    param.size = 16;
    param.reserved = 1;
    Require(sceDeviceServiceInitialize(3, &param) == invalidArgument, "reserved field accepted");
    param.reserved = 0;
    DeviceServiceThreadParam threadParam{};
    threadParam.reserved = 1;
    param.threadParam = &threadParam;
    Require(sceDeviceServiceInitialize(3, &param) == invalidArgument, "threadParam reserved field accepted");
    param.threadParam = nullptr;
    Require(sceDeviceServiceInitialize(3, &param) == 0, "default parameter initialization failed");
    Require(sceDeviceServiceInitialize(3, 0) == 0, "initialization failed");
    Require(sceDeviceServiceGetGeneration() == 0, "the device set started at another generation");
    Require(sceDeviceServiceGetEventState(1) == 0, "a device event was reported");
    std::array<std::uint8_t, 0x70> info{};
    std::int32_t count = 7;
    std::int32_t reserved = 0;
    Require(sceDeviceServiceQueryDeviceInfo_(0x7001, 0, 0, info.data(), 1, &count, &reserved, info.size()) == 0 && count == 0, "a device was listed");
    Require(sceDeviceServiceQueryDeviceInfo_(0x7001, nullptr, 0, info.data(), 1, nullptr, &reserved, info.size()) == 0, "optional count output rejected");
    Require(sceDeviceServiceQueryDeviceInfo_(0x7001, nullptr, 0, nullptr, 0, nullptr, nullptr, info.size()) == 0, "empty query rejected");
    Require(sceDeviceServiceQueryDeviceInfo_(0x7001, nullptr, 0, nullptr, 1, &count, nullptr, info.size()) == invalidArgument, "null record buffer accepted");
    Require(sceDeviceServiceQueryDeviceInfo_(0x7001, nullptr, 0, info.data(), -1, &count, nullptr, info.size()) == invalidArgument, "negative capacity accepted");
    Require(sceDeviceServiceQueryDeviceInfo_(0x7001, nullptr, 0, info.data(), 1, &count, nullptr, info.size() - 1) == invalidArgument, "incorrect record size accepted");
    Require(sceDeviceServiceQueryDeviceInfo_(0, nullptr, 0, info.data(), 1, &count, nullptr, info.size()) == invalidArgument, "invalid device class accepted");
    Require(sceDeviceServiceQueryDeviceInfo_(0xFFFF, nullptr, 0, info.data(), 1, &count, nullptr, info.size()) == invalidArgument, "unrecognized device class accepted");
    DeviceServiceUsageFilter filter{1, 2, 3};
    Require(sceDeviceServiceQueryDeviceInfo_(0x8001, &filter, 0, info.data(), 1, &count, nullptr, info.size()) == 0, "valid HID filter rejected");
    Require(sceDeviceServiceQueryDeviceInfo_(0x8001, nullptr, 0, info.data(), 1, &count, nullptr, info.size()) == invalidArgument, "missing HID filter accepted");
    Require(sceDeviceServiceQueryDeviceInfo_(0x7001, &filter, 0, info.data(), 1, &count, nullptr, info.size()) == invalidArgument, "unexpected HID filter accepted");
    filter.minimumUsage = 4;
    Require(sceDeviceServiceQueryDeviceInfo_(0x8001, &filter, 0, info.data(), 1, &count, nullptr, info.size()) == invalidArgument, "reversed HID usage range accepted");
    Require(sceDeviceServiceGetGeneration() == 0, "the generation advanced without a device change");
    Require(sceDeviceServiceTerminate() == 0, "termination failed");
    std::puts("DeviceService tests passed");
    return 0;
}
