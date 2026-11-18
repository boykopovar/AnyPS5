#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr int PAD_ERROR_DEVICE_NOT_CONNECTED = static_cast<int>(0x80920007);

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

} // namespace

int main() {
    for (const int handle : {-9999, -100, -1, 0, 1, PAD_REMOTE_HANDLE, 100, 12345}) {
        Require(scePadSetMotionSensorState(handle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetMotionSensorState(handle, false) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadResetOrientation(handle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetTiltCorrectionState(handle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetTiltCorrectionState(handle, false) == PAD_ERROR_INVALID_HANDLE);
    }

    PadMotionSensorPosition uninitPos{};
    float uninitTimerUnit = 0.0f;
    Require(scePadGetMotionSensorPosition(0, nullptr) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadGetMotionSensorPosition(0, &uninitPos) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadGetMotionSensorPosition(12345, &uninitPos) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadGetMotionSensorPosition(12345, reinterpret_cast<PadMotionSensorPosition*>(0x10)) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadGetMotionTimerUnit(0, nullptr) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadGetMotionTimerUnit(0, &uninitTimerUnit) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadGetMotionTimerUnit(12345, &uninitTimerUnit) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadGetMotionTimerUnit(12345, reinterpret_cast<float*>(0x10)) == PAD_ERROR_NOT_INITIALIZED);

    Require(scePadInit_nid_postfix() == PAD_OK);

    for (const int handle : {-9999, -100, -1, 0, 1, PAD_REMOTE_HANDLE, 100, 12345}) {
        Require(scePadSetMotionSensorState(handle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetMotionSensorState(handle, false) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadResetOrientation(handle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetTiltCorrectionState(handle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetTiltCorrectionState(handle, false) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadGetMotionSensorPosition(0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetMotionSensorPosition(12345, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetMotionSensorPosition(0, reinterpret_cast<PadMotionSensorPosition*>(0x10)) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetMotionSensorPosition(12345, reinterpret_cast<PadMotionSensorPosition*>(0x10)) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetMotionTimerUnit(0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetMotionTimerUnit(12345, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetMotionTimerUnit(0, reinterpret_cast<float*>(0x10)) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetMotionTimerUnit(12345, reinterpret_cast<float*>(0x10)) == PAD_ERROR_INVALID_ARG);

    for (const int unopenedHandle : {-9999, -1, 0, 1, PAD_REMOTE_HANDLE, 12345}) {
        PadMotionSensorPosition pos{};
        float unit = 1.0f;
        Require(scePadGetMotionSensorPosition(unopenedHandle, &pos) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadGetMotionTimerUnit(unopenedHandle, &unit) == PAD_ERROR_INVALID_HANDLE);
        Require(unit == 0.0f);
    }

    const int handle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle > 0);

    Require(scePadSetMotionSensorState(handle, true) == PAD_OK);
    Require(scePadSetMotionSensorState(handle, false) == PAD_OK);
    Require(scePadResetOrientation(handle) == PAD_OK);
    Require(scePadSetTiltCorrectionState(handle, true) == PAD_OK);
    Require(scePadSetTiltCorrectionState(handle, false) == PAD_OK);

    Require(scePadGetMotionSensorPosition(handle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetMotionSensorPosition(handle, reinterpret_cast<PadMotionSensorPosition*>(0x10)) == PAD_ERROR_INVALID_ARG);
    PadMotionSensorPosition activePos{1.0f, 2.0f, 3.0f};
    Require(scePadGetMotionSensorPosition(handle, &activePos) == PAD_ERROR_DEVICE_NOT_CONNECTED);

    Require(scePadGetMotionTimerUnit(handle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetMotionTimerUnit(handle, reinterpret_cast<float*>(0x10)) == PAD_ERROR_INVALID_ARG);
    float activeUnit = 5.0f;
    Require(scePadGetMotionTimerUnit(handle, &activeUnit) == PAD_ERROR_DEVICE_NOT_CONNECTED);
    Require(activeUnit == 0.0f);

    PadInputState moving{};
    moving.hasMotion = true;
    moving.accel = {0.0f, 9.80665f, 0.0f};
    moving.gyro = {0.125f, -0.250f, 0.500f};
    PadPublishInput_nid_postfix(moving);

    Require(scePadSetMotionSensorState(handle, true) == PAD_OK);
    PadData liveData{};
    Require(scePadReadState(handle, &liveData) == PAD_OK);
    Require(liveData.acceleration_x == 0.0f);
    Require(liveData.acceleration_y == 1.0f);
    Require(liveData.acceleration_z == 0.0f);
    Require(liveData.angular_velocity_x == 0.125f);
    Require(liveData.angular_velocity_y == -0.250f);
    Require(liveData.angular_velocity_z == 0.500f);

    Require(scePadSetMotionSensorState(handle, false) == PAD_OK);
    PadData disabledData{};
    Require(scePadReadState(handle, &disabledData) == PAD_OK);
    Require(disabledData.acceleration_x == 0.0f);
    Require(disabledData.acceleration_y == 1.0f);
    Require(disabledData.acceleration_z == 0.0f);
    Require(disabledData.angular_velocity_x == 0.0f);
    Require(disabledData.angular_velocity_y == 0.0f);
    Require(disabledData.angular_velocity_z == 0.0f);
    Require(disabledData.orientation_x == 0.0f);
    Require(disabledData.orientation_y == 0.0f);
    Require(disabledData.orientation_z == 0.0f);
    Require(disabledData.orientation_w == 0.0f);

    Require(scePadSetMotionSensorState(handle, true) == PAD_OK);
    PadData restoredData{};
    Require(scePadReadState(handle, &restoredData) == PAD_OK);
    Require(restoredData.angular_velocity_x == 0.125f);
    Require(restoredData.angular_velocity_y == -0.250f);
    Require(restoredData.angular_velocity_z == 0.500f);

    Require(scePadResetOrientation(handle) == PAD_OK);
    PadPublishInput_nid_postfix(PadInputState{});

    const int handle2 = scePadOpen_nid_postfix(0x10000001, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle2 > 0);
    Require(scePadSetMotionSensorState(handle2, true) == PAD_OK);
    Require(scePadSetMotionSensorState(handle2, false) == PAD_OK);
    Require(scePadResetOrientation(handle2) == PAD_OK);
    Require(scePadSetTiltCorrectionState(handle2, true) == PAD_OK);
    Require(scePadSetTiltCorrectionState(handle2, false) == PAD_OK);
    Require(scePadClose_nid_postfix(handle2) == PAD_OK);

    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);
    Require(scePadSetMotionSensorState(special, true) == PAD_OK);
    Require(scePadSetMotionSensorState(special, false) == PAD_OK);
    Require(scePadResetOrientation(special) == PAD_OK);
    Require(scePadSetTiltCorrectionState(special, true) == PAD_OK);
    Require(scePadSetTiltCorrectionState(special, false) == PAD_OK);

    PadMotionSensorPosition specialPos{};
    float specialUnit = 9.0f;
    Require(scePadGetMotionSensorPosition(special, &specialPos) == PAD_ERROR_DEVICE_NOT_CONNECTED);
    Require(scePadGetMotionTimerUnit(special, &specialUnit) == PAD_ERROR_DEVICE_NOT_CONNECTED);
    Require(specialUnit == 0.0f);

    Require(scePadClose_nid_postfix(special) == PAD_OK);
    Require(scePadSetMotionSensorState(special, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetMotionSensorState(special, false) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadResetOrientation(special) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetTiltCorrectionState(special, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetTiltCorrectionState(special, false) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadGetMotionSensorPosition(special, &specialPos) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadGetMotionTimerUnit(special, &specialUnit) == PAD_ERROR_INVALID_HANDLE);

    Require(scePadClose_nid_postfix(handle) == PAD_OK);
    Require(scePadGetMotionSensorPosition(handle, &activePos) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadGetMotionTimerUnit(handle, &activeUnit) == PAD_ERROR_INVALID_HANDLE);

#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::size_t pageSize = info.dwPageSize;
    void* page = VirtualAlloc(nullptr, pageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READONLY);
    Require(page != nullptr);
#else
    const long size = sysconf(_SC_PAGESIZE);
    Require(size > 0);
    const std::size_t pageSize = static_cast<std::size_t>(size);
    void* page = mmap(nullptr, pageSize, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED);
#endif
    auto* const roPos = static_cast<PadMotionSensorPosition*>(page);
    auto* const roUnit = static_cast<float*>(page);
    Require(scePadGetMotionSensorPosition(12345, roPos) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetMotionTimerUnit(12345, roUnit) == PAD_ERROR_INVALID_ARG);
#ifdef _WIN32
    Require(VirtualFree(page, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(page, pageSize) == 0);
#endif

    return 0;
}
