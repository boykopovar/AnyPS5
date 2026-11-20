#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <array>
#include <cmath>
#include <cstdlib>
#include <initializer_list>

namespace {

constexpr int PAD_ERROR_DEVICE_NOT_CONNECTED = static_cast<int>(0x80920007u);

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestPreInitializationBounds() {
    PadMotionSensorPosition position{};
    float unit = 1.0f;
    float radius = 1.0f;

    for (int handle : {-100, -1, 0, 1, 2, 3, 4, 5, 10, PAD_REMOTE_HANDLE, PAD_SPECIAL_HANDLE, 12345}) {
        Require(scePadResetOrientation(handle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetMotionSensorState(handle, false) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetMotionSensorState(handle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadGetMotionSensorPosition(handle, nullptr) == PAD_ERROR_NOT_INITIALIZED);
        Require(scePadGetMotionSensorPosition(handle, &position) == PAD_ERROR_NOT_INITIALIZED);
        Require(scePadGetMotionTimerUnit(handle, nullptr) == PAD_ERROR_NOT_INITIALIZED);
        Require(scePadGetMotionTimerUnit(handle, &unit) == PAD_ERROR_NOT_INITIALIZED);
        Require(scePadGetSphereRadius(handle, nullptr) == PAD_ERROR_NOT_INITIALIZED);
        Require(scePadGetSphereRadius(handle, &radius) == PAD_ERROR_NOT_INITIALIZED);
    }

    bool caught = false;
    try {
        scePadResetOrientationForTracker(1);
    } catch (...) {
        caught = true;
    }
    Require(caught);
}

void TestPostInitializationUnopenedHandleBounds() {
    Require(scePadInit_nid_postfix() == PAD_OK);

    PadMotionSensorPosition position{};
    float unit = 1.0f;
    float radius = 1.0f;

    for (int handle : {-100, -1, 0, 1, 2, 3, 4, 5, 10, PAD_REMOTE_HANDLE, PAD_SPECIAL_HANDLE, 12345}) {
        Require(scePadResetOrientation(handle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetMotionSensorState(handle, false) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetMotionSensorState(handle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadGetMotionSensorPosition(handle, nullptr) == PAD_ERROR_INVALID_ARG);
        Require(scePadGetMotionSensorPosition(handle, &position) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadGetMotionTimerUnit(handle, nullptr) == PAD_ERROR_INVALID_ARG);
        Require(scePadGetMotionTimerUnit(handle, &unit) == PAD_ERROR_INVALID_HANDLE);
        Require(unit == 0.0f);
        unit = 1.0f;
        Require(scePadGetSphereRadius(handle, nullptr) == PAD_ERROR_INVALID_ARG);
        Require(scePadGetSphereRadius(handle, &radius) == PAD_ERROR_INVALID_HANDLE);
    }
}

void TestStandardHandleMotionSensorQueriesAndResetOrientation() {
    const int handle0 = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle0 == 1);

    PadMotionSensorPosition position{};
    float unit = 1.0f;
    float radius = 1.0f;

    Require(scePadGetMotionSensorPosition(handle0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetMotionSensorPosition(handle0, &position) == PAD_ERROR_DEVICE_NOT_CONNECTED);

    Require(scePadGetMotionTimerUnit(handle0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetMotionTimerUnit(handle0, &unit) == PAD_ERROR_DEVICE_NOT_CONNECTED);
    Require(unit == 0.0f);

    Require(scePadGetSphereRadius(handle0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetSphereRadius(handle0, &radius) == PAD_ERROR_INVALID_HANDLE);

    Require(scePadSetMotionSensorState(handle0, false) == PAD_OK);
    Require(scePadSetMotionSensorState(handle0, true) == PAD_OK);

    Require(scePadResetOrientation(handle0) == PAD_OK);
    PadData data{};
    Require(scePadReadState(handle0, &data) == PAD_OK);
    Require(data.orientation_w == 1.0f);
    Require(data.orientation_x == 0.0f);
    Require(data.orientation_y == 0.0f);
    Require(data.orientation_z == 0.0f);

    for (int invalid : {-1, 0, 2, 3, 4, 5, PAD_REMOTE_HANDLE, PAD_SPECIAL_HANDLE, 999}) {
        Require(scePadResetOrientation(invalid) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetMotionSensorState(invalid, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadGetMotionSensorPosition(invalid, nullptr) == PAD_ERROR_INVALID_ARG);
        Require(scePadGetMotionSensorPosition(invalid, &position) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadGetMotionTimerUnit(invalid, nullptr) == PAD_ERROR_INVALID_ARG);
        Require(scePadGetMotionTimerUnit(invalid, &unit) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadGetSphereRadius(invalid, nullptr) == PAD_ERROR_INVALID_ARG);
    }
}

void TestOrientationResetUnderActiveMotion() {
    const int handle0 = 1;

    PadInputState moving{};
    moving.hasMotion = true;
    moving.accel = {9.80665f, 0.0f, 0.0f};
    moving.gyro = {0.05f, 0.05f, 0.05f};
    PadPublishInputForPort_nid_postfix(0, moving);

    PadData data{};
    Require(scePadReadState(handle0, &data) == PAD_OK);

    PadInputState stationary{};
    PadPublishInputForPort_nid_postfix(0, stationary);

    Require(scePadResetOrientation(handle0) == PAD_OK);
    Require(scePadReadState(handle0, &data) == PAD_OK);
    Require(std::abs(data.orientation_w - 1.0f) < 0.01f);
    Require(std::abs(data.orientation_x) < 0.01f);
    Require(std::abs(data.orientation_y) < 0.01f);
    Require(std::abs(data.orientation_z) < 0.01f);
}

void TestSpecialAndRemotePortBehavior() {
    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special == PAD_SPECIAL_HANDLE);

    PadMotionSensorPosition position{};
    float unit = 1.0f;
    float radius = 1.0f;

    Require(scePadResetOrientation(special) == PAD_OK);
    Require(scePadSetMotionSensorState(special, true) == PAD_OK);
    Require(scePadSetMotionSensorState(special, false) == PAD_OK);

    Require(scePadGetMotionSensorPosition(special, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetMotionSensorPosition(special, &position) == PAD_ERROR_DEVICE_NOT_CONNECTED);

    Require(scePadGetMotionTimerUnit(special, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetMotionTimerUnit(special, &unit) == PAD_ERROR_DEVICE_NOT_CONNECTED);

    Require(scePadGetSphereRadius(special, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetSphereRadius(special, &radius) == PAD_ERROR_DEVICE_NOT_CONNECTED);

    Require(scePadClose_nid_postfix(special) == PAD_OK);
    Require(scePadResetOrientation(special) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetMotionSensorState(special, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadGetMotionSensorPosition(special, &position) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadGetMotionTimerUnit(special, &unit) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadGetSphereRadius(special, &radius) == PAD_ERROR_INVALID_HANDLE);

    const int remote = scePadOpen_nid_postfix(PAD_USER_ID_SYSTEM, PAD_PORT_TYPE_REMOTE, 0, nullptr);
    Require(remote == PAD_REMOTE_HANDLE);

    Require(scePadResetOrientation(remote) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetMotionSensorState(remote, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadGetMotionSensorPosition(remote, &position) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadGetMotionTimerUnit(remote, &unit) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadGetSphereRadius(remote, &radius) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadClose_nid_postfix(remote) == PAD_OK);
}

void TestTrackerResetOrientationThrows() {
    const int handle0 = 1;
    bool caught = false;
    try {
        scePadResetOrientationForTracker(handle0);
    } catch (...) {
        caught = true;
    }
    Require(caught);
}

void TestClosedStandardHandleBehavior() {
    const int handle0 = 1;
    const int handle1 = scePadOpen_nid_postfix(0x10000001, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle1 == 2);

    PadMotionSensorPosition position{};
    float unit = 1.0f;

    Require(scePadClose_nid_postfix(handle1) == PAD_OK);
    Require(scePadResetOrientation(handle1) == PAD_OK);
    Require(scePadSetMotionSensorState(handle1, true) == PAD_OK);
    Require(scePadSetMotionSensorState(handle1, false) == PAD_OK);

    Require(scePadGetMotionSensorPosition(handle1, &position) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadGetMotionTimerUnit(handle1, &unit) == PAD_ERROR_INVALID_HANDLE);

    Require(scePadResetOrientation(3) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetMotionSensorState(3, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadGetMotionSensorPosition(3, &position) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadGetMotionTimerUnit(3, &unit) == PAD_ERROR_INVALID_HANDLE);

    PadPublishInputForPort_nid_postfix(0, PadInputState{});
    Require(scePadClose_nid_postfix(handle0) == PAD_OK);
}

} // namespace

int main() {
    TestPreInitializationBounds();
    TestPostInitializationUnopenedHandleBounds();
    TestStandardHandleMotionSensorQueriesAndResetOrientation();
    TestOrientationResetUnderActiveMotion();
    TestSpecialAndRemotePortBehavior();
    TestTrackerResetOrientationThrows();
    TestClosedStandardHandleBehavior();
    return 0;
}
