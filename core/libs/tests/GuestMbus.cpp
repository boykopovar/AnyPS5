#include "prx/libSceMbus/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>

static_assert(std::is_same_v<decltype(&sceMbusEventCallbackFuncsInit_), void (APS5_VABI *)(MbusEventCallbacks*, std::uint32_t)>);

namespace {

void ClobberErrno(std::uintptr_t, std::size_t) { errno = 99; }

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "DeviceService: %s\n", message);
        std::abort();
    }
}

void CheckDeviceIdConversion() {
    constexpr auto unconvertibleClass = static_cast<std::int32_t>(0x809B0008u);
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
    for (const std::uint32_t other : {0u, 1u, 0x11u, 0x21u, 0x30u, 0x1000u, 0xFFFFFFFFu}) {
        id = 7;
        Require(sceMbusConvertToMbusDeviceId(1, other, &id) == unconvertibleClass && id == 0, "other classes are refused and clear the output");
    }
    Require(sceMbusConvertToMbusDeviceId(1, 0, nullptr) == unconvertibleClass, "refusal without an output");
    constexpr auto invalidArgument = static_cast<std::int32_t>(0x809B0080u);
    errno = 71;
    Require(sceMbusConvertToLocalDeviceId(0x0020123456789ABCull, reinterpret_cast<std::uint32_t*>(0x10), &deviceClass) == invalidArgument, "unwritable local id accepted");
    Require(errno == 71, "errno modified by unwritable local id check");
    Require(sceMbusConvertToLocalDeviceId(0x0020123456789ABCull, &local, reinterpret_cast<std::uint32_t*>(0x10)) == invalidArgument, "unwritable device class accepted");
    Require(errno == 71, "errno modified by unwritable device class check");
    Require(sceMbusConvertToLocalDeviceId2(0x0010123456789ABCull, 0x10, reinterpret_cast<std::uint64_t*>(0x10)) == invalidArgument, "unwritable local id 2 accepted");
    Require(errno == 71, "errno modified by unwritable local id 2 check");
    Require(sceMbusConvertToMbusDeviceId(0x123456789ABCull, 0x20, reinterpret_cast<std::uint64_t*>(0x10)) == invalidArgument, "unwritable mbus device id accepted");
    Require(errno == 71, "errno modified by unwritable mbus device id check");
}

void CheckEmptyDeviceQueries() {
    constexpr auto invalidArgument = static_cast<std::int32_t>(0x809B0080u);
    MbusDeviceCondition condition{};
    alignas(16) std::array<std::uint8_t, 256> records{};
    records.fill(0xa5);
    std::int32_t first = 7;
    std::int32_t second = 9;
    auto* infos = reinterpret_cast<MbusDeviceInfo*>(records.data());
    Require(sceMbusGetDeviceInfoByCondition_(&condition, 3, infos, 1, &first, &second, sizeof(condition)) == 0, "condition query failed");
    Require(first == 0 && second == 0, "condition query reported a device");
    Require(records[0] == 0xa5 && records[255] == 0xa5, "condition query wrote a record");
    first = 7;
    Require(sceMbusGetDeviceInfoByCondition_(&condition, 1, nullptr, 0, &first, nullptr, sizeof(condition)) == 0 && first == 0, "empty condition query rejected");
    Require(sceMbusGetDeviceInfoByCondition_(nullptr, 0, infos, 1, &first, &second, sizeof(condition)) == invalidArgument, "null condition accepted");
    Require(sceMbusGetDeviceInfoByCondition_(&condition, 0, nullptr, 1, &first, &second, sizeof(condition)) == invalidArgument, "null record buffer accepted");
    Require(sceMbusGetDeviceInfoByCondition_(&condition, 0, infos, -1, &first, &second, sizeof(condition)) == invalidArgument, "negative capacity accepted");
    Require(sceMbusGetDeviceInfoByCondition_(&condition, 0, infos, 1, &first, &second, 112) == invalidArgument, "wrong record size accepted");
    errno = 71;
    Require(sceMbusGetDeviceInfoByCondition_(&condition, 0, infos, 1, reinterpret_cast<std::int32_t*>(0x10), &second, sizeof(condition)) == invalidArgument, "unwritable count accepted");
    Require(errno == 71, "condition query altered errno");
    Require(sceMbusGetDeviceInfoByCondition_(reinterpret_cast<const MbusDeviceCondition*>(0x10), 0, infos, 0, &first, &second, sizeof(condition)) == invalidArgument, "unreadable condition accepted");

    alignas(16) std::array<std::uint8_t, 112> serviceRecords{};
    serviceRecords.fill(0xa5);
    first = 4;
    second = 5;
    auto* serviceInfos = reinterpret_cast<MbusDeviceServiceInfo*>(serviceRecords.data());
    Require(sceMbusGetDeviceInfoByConditionForDeviceService(0, serviceInfos, 1, &first, &second, condition) == 0, "service condition query failed");
    Require(first == 0 && second == 0, "service condition query reported a device");
    Require(serviceRecords[0] == 0xa5 && serviceRecords[111] == 0xa5, "service condition query wrote a record");
    Require(sceMbusGetDeviceInfoByConditionForDeviceService(0, nullptr, 0, nullptr, nullptr, condition) == 0, "empty service query rejected");
    Require(sceMbusGetDeviceInfoByConditionForDeviceService(0, nullptr, 1, &first, &second, condition) == invalidArgument, "null service buffer accepted");
    Require(sceMbusGetDeviceInfoByConditionForDeviceService(0, serviceInfos, -1, &first, &second, condition) == invalidArgument, "negative service capacity accepted");
    errno = 71;
    Require(sceMbusGetDeviceInfoByConditionForDeviceService(0, serviceInfos, 1, reinterpret_cast<std::int32_t*>(0x10), &second, condition) == invalidArgument, "unwritable service count accepted");
    Require(errno == 71, "service condition query altered errno");
}

void CheckInit() {
    Require(sceMbusInit() == 0, "the first initialization failed");
    Require(sceMbusInit() == 0, "a second initialization failed");
    Require(sceMbusTerm() == 0, "termination failed");
    Require(sceMbusInit() == 0, "an initialization after termination failed");
    Require(sceMbusTerm() == 0, "a second termination failed");
}

}

int main() {
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);
    static_assert(sizeof(MbusEventCallbacks) == 16);
    sceMbusEventCallbackFuncsInit_(nullptr, sizeof(MbusEventCallbacks));
    errno = 71;
    sceMbusEventCallbackFuncsInit_(reinterpret_cast<MbusEventCallbacks*>(0x10), sizeof(MbusEventCallbacks));
    Require(errno == 71, "unwritable callback initialization altered errno");
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
    std::array<std::uint8_t, 532> longCallbacks{};
    longCallbacks.fill(0xa5);
    sceMbusEventCallbackFuncsInit_(reinterpret_cast<MbusEventCallbacks*>(longCallbacks.data()), 516);
    for (std::size_t i = 0; i < longCallbacks.size(); ++i) Require(longCallbacks[i] == (i < 516 ? 0 : 0xa5), "large callback initialization wrote outside the requested range");

    Require(sceMbusAcquireControlList() == static_cast<int>(0x809B00FFu), "control-list fallback changed");
    CheckDeviceIdConversion();
    CheckEmptyDeviceQueries();
    CheckInit();
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
    errno = 71;
    Require(sceDeviceServiceInitialize(3, &param) == 0, "default parameter initialization failed");
    Require(errno == 71, "errno modified by valid parameter initialization");
    errno = 71;
    Require(sceDeviceServiceInitialize(3, reinterpret_cast<const DeviceServiceInitParam*>(1)) == invalidArgument, "unmapped parameter accepted");
    Require(errno == 71, "errno modified by unmapped parameter initialization");
    param.threadParam = reinterpret_cast<DeviceServiceThreadParam*>(~std::uintptr_t{0});
    errno = 71;
    Require(sceDeviceServiceInitialize(3, &param) == invalidArgument, "unmapped thread parameter accepted");
    Require(errno == 71, "errno modified by unmapped thread parameter initialization");
    param.threadParam = nullptr;
    Require(sceDeviceServiceInitialize(3, 0) == 0, "initialization failed");
    Require(sceDeviceServiceGetGeneration() == 0, "the device set started at another generation");
    Require(sceDeviceServiceGetEventState(1) == 0, "a device event was reported");
    std::array<std::uint8_t, 0x70> info{};
    std::int32_t count = 7;
    std::int32_t reserved = 0;
    Require(sceDeviceServiceQueryDeviceInfo_(0x7001, 0, 0, info.data(), 1, &count, &reserved, info.size()) == 0 && count == 0, "a device was listed");
    Require(sceDeviceServiceQueryDeviceInfo_(0x7001, nullptr, 0, info.data(), 1, reinterpret_cast<std::int32_t*>(0x10), nullptr, info.size()) == invalidArgument, "unwritable count accepted");
    Require(sceDeviceServiceQueryDeviceInfo_(0x7001, nullptr, 0, info.data(), 1, nullptr, &reserved, info.size()) == 0, "optional count output rejected");
    errno = 71;
    Require(sceDeviceServiceQueryDeviceInfo_(0x7001, nullptr, 0, info.data(), 1, reinterpret_cast<std::int32_t*>(0x10), &reserved, info.size()) == invalidArgument, "unwritable count pointer accepted");
    Require(errno == 71, "errno modified by unwritable pointer check");
    Require(sceDeviceServiceQueryDeviceInfo_(0x7001, nullptr, 0, nullptr, 0, nullptr, nullptr, info.size()) == 0, "empty query rejected");
    Require(sceDeviceServiceQueryDeviceInfo_(0x7001, nullptr, 0, nullptr, 1, &count, nullptr, info.size()) == invalidArgument, "null record buffer accepted");
    Require(sceDeviceServiceQueryDeviceInfo_(0x7001, nullptr, 0, info.data(), -1, &count, nullptr, info.size()) == invalidArgument, "negative capacity accepted");
    Require(sceDeviceServiceQueryDeviceInfo_(0x7001, nullptr, 0, info.data(), 1, &count, nullptr, info.size() - 1) == invalidArgument, "incorrect record size accepted");
    Require(sceDeviceServiceQueryDeviceInfo_(0, nullptr, 0, info.data(), 1, &count, nullptr, info.size()) == invalidArgument, "invalid device class accepted");
    Require(sceDeviceServiceQueryDeviceInfo_(0xFFFF, nullptr, 0, info.data(), 1, &count, nullptr, info.size()) == invalidArgument, "unrecognized device class accepted");
    DeviceServiceUsageFilter filter{1, 2, 3};
    Require(sceDeviceServiceQueryDeviceInfo_(0x8001, &filter, 0, info.data(), 1, &count, nullptr, info.size()) == 0, "valid HID filter rejected");
    count = 7;
    Require(sceDeviceServiceQueryDeviceInfo_(0x8001, reinterpret_cast<const DeviceServiceUsageFilter*>(0x10), 0, info.data(), 1, &count, nullptr, info.size()) == invalidArgument && count == 7,
            "unreadable HID filter accepted or modified the count output");
    Require(sceDeviceServiceQueryDeviceInfo_(0x8001, nullptr, 0, info.data(), 1, &count, nullptr, info.size()) == invalidArgument, "missing HID filter accepted");
    Require(sceDeviceServiceQueryDeviceInfo_(0x7001, &filter, 0, info.data(), 1, &count, nullptr, info.size()) == invalidArgument, "unexpected HID filter accepted");
    filter.minimumUsage = 4;
    Require(sceDeviceServiceQueryDeviceInfo_(0x8001, &filter, 0, info.data(), 1, &count, nullptr, info.size()) == invalidArgument, "reversed HID usage range accepted");
    Require(sceDeviceServiceGetGeneration() == 0, "the generation advanced without a device change");
    Require(sceDeviceServiceTerminate() == 0, "termination failed");
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    std::puts("DeviceService tests passed");
    return 0;
}
