#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>

using ReadHistoryFn = int (APS5_VABI*)(std::int32_t, PadHistoryData*, std::uint32_t);
using OpenExt2Fn = int (APS5_VABI*)(std::int32_t, std::int32_t, const void*);

static void Require(bool value) { if (!value) std::abort(); }

template <typename Function, typename Export>
static Function As(Export function) {
    return reinterpret_cast<Function>(function);
}

int main() {
    const auto readHistory = As<ReadHistoryFn>(&scePadReadHistory);
    const auto openExt2 = As<OpenExt2Fn>(&scePadOpenExt2);
    alignas(8) std::array<std::uint8_t, 0x960> buffer{};
    std::array<std::uint8_t, 0x10> param{};
    std::uint32_t capability = 7;
    std::uint64_t count = 7;
    std::uint64_t activeTime = 7;
    std::int32_t deviceId = 7;
    auto* data = buffer.data();

    Require(scePadGetDeviceInfo(0, reinterpret_cast<PadDeviceInfo*>(data)) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadGetCapability(0, &capability) == PAD_ERROR_NOT_INITIALIZED && capability == 7);
    Require(scePadGetVersionInfo(1, reinterpret_cast<PadVersionInfo*>(data)) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadGetIdleCount(1) == 0);
    Require(scePadGetControllerIdleCount(1, &count) == PAD_ERROR_NOT_INITIALIZED && count == 7);
    Require(scePadGetActiveTime(1, 0, &activeTime) == PAD_ERROR_NOT_INITIALIZED && activeTime == 7);
    Require(scePadSetPS4BcVibrationMode(0) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadGetInfoByPortType(1, reinterpret_cast<PadPortInfo*>(data)) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadGetInfoByPortType(0, reinterpret_cast<PadPortInfo*>(data)) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadGetInfoByPortType(2, nullptr) == PAD_ERROR_INVALID_ARG && scePadGetInfoByPortType(1, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetDeviceId(1, &deviceId) == PAD_ERROR_NOT_INITIALIZED && deviceId == 7);
    Require(scePadGetExtensionUnitInfo(1, reinterpret_cast<PadExtensionUnitInfo*>(data)) == PAD_ERROR_NOT_INITIALIZED);
    Require(readHistory(1, reinterpret_cast<PadHistoryData*>(data), 0x960) == PAD_ERROR_NOT_INITIALIZED);
    Require(openExt2(0x10000000, 0x10, param.data()) == PAD_ERROR_NOT_INITIALIZED);

    Require(scePadInit_nid_postfix() == PAD_OK);
    Require(scePadGetDeviceInfo(0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetCapability(0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetVersionInfo(1, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetControllerIdleCount(0, &count) == PAD_ERROR_INVALID_ARG && scePadGetControllerIdleCount(1, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetActiveTime(1, 0, nullptr) == PAD_ERROR_INVALID_ARG);
    for (int type : {1, 3, 4, -1}) Require(scePadGetInfoByPortType(type, reinterpret_cast<PadPortInfo*>(data)) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetInfoByPortType(1, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetDeviceId(1, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetDeviceId(12345, &deviceId) == PAD_ERROR_INVALID_HANDLE && deviceId == -1);
    Require(scePadGetExtensionUnitInfo(1, nullptr) == PAD_ERROR_INVALID_ARG);
    for (std::uint32_t size : {0u, 0x48u, 0x958u, 0x961u}) Require(readHistory(1, reinterpret_cast<PadHistoryData*>(data), size) == PAD_ERROR_INVALID_ARG);
    Require(openExt2(0x10000000, 0x8, param.data()) == PAD_ERROR_INVALID_ARG && openExt2(0x10000000, 0x10, nullptr) == PAD_ERROR_INVALID_ARG);

    const int handle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle > 0);
    Require(scePadReadStateExt(handle, nullptr) == PAD_ERROR_INVALID_ARG);
    return 0;
}
