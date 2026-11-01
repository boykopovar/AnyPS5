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

void CheckDeviceIdConversion() {
    std::uint32_t local = 7;
    std::uint32_t deviceClass = 7;
    Require(sceMbusConvertToLocalDeviceId(0x0020123456789ABCull, &local, &deviceClass) == 0, "local conversion failed");
    Require(local == 0x56789ABC && deviceClass == 0x20, "the local id is the low 32 bits and the class the top 16");
    Require(sceMbusConvertToLocalDeviceId(0xFFFF000000000001ull, nullptr, &deviceClass) == 0 && deviceClass == 0xFFFF, "class without a local output");
    Require(sceMbusConvertToLocalDeviceId(0xFFFF000000000001ull, &local, nullptr) == 0 && local == 1, "local id without a class output");
    Require(sceMbusConvertToLocalDeviceId(0, nullptr, nullptr) == 0, "conversion without outputs");
    std::uint64_t id = 7;
    Require(sceMbusConvertToLocalDeviceId2(0x0010123456789ABCull, 0x10, &id) == 0 && id == 0x123456789ABCull, "the same class keeps the low 48 bits");
    Require(sceMbusConvertToLocalDeviceId2(0x0010123456789ABCull, 0x10, nullptr) == 0, "same class without an output");
    bool threw = false;
    try {
        sceMbusConvertToLocalDeviceId2(0x0010123456789ABCull, 0x20, &id);
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw && id == 0x123456789ABCull, "another class needs the Mbus service");
    Require(sceMbusConvertToMbusDeviceId(0x123456789ABCull, 0x20, &id) == 0 && id == 0x0020123456789ABCull, "class 0x20 is placed in the top 16 bits");
    Require(sceMbusConvertToMbusDeviceId(0xFFFF000000000001ull, 0x10, &id) == 0 && id == 0xFFFF000000000001ull, "the local id is not masked");
    Require(sceMbusConvertToMbusDeviceId(1, 0x10, nullptr) == 0, "conversion without an output");
    constexpr auto unconvertibleClass = static_cast<std::int32_t>(0x809B0008u);
    for (const std::uint32_t other : {0u, 1u, 0x11u, 0x21u, 0x30u, 0x1000u, 0xFFFFFFFFu}) {
        id = 7;
        Require(sceMbusConvertToMbusDeviceId(1, other, &id) == unconvertibleClass && id == 0, "other classes are refused and clear the output");
    }
    Require(sceMbusConvertToMbusDeviceId(1, 0, nullptr) == unconvertibleClass, "refusal without an output");
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

    CheckDeviceIdConversion();
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
