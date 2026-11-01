#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include <cstdlib>
#include <initializer_list>

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

static inline int scePadSetAngularVelocityDeadband(int handle, bool enable) {
    return scePadSetAngularVelocityDeadbandState(handle, enable);
}

int main() {
    for (int handle : {-9999, -100, -2, -1, 0, 5, 99, 1000}) {
        Require(scePadResetOrientation(handle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadband(handle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadband(handle, false) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadbandState(handle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadbandState(handle, false) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadInit_nid_postfix() == PAD_OK);

    for (int handle : {-9999, -100, -2, -1, 0, 5, 99, 1000}) {
        Require(scePadResetOrientation(handle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadband(handle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadband(handle, false) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadbandState(handle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadbandState(handle, false) == PAD_ERROR_INVALID_HANDLE);
    }

    const int handle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle > 0);

    Require(scePadResetOrientation(handle) == PAD_OK);
    Require(scePadSetAngularVelocityDeadband(handle, true) == PAD_OK);
    Require(scePadSetAngularVelocityDeadband(handle, false) == PAD_OK);
    Require(scePadSetAngularVelocityDeadbandState(handle, true) == PAD_OK);
    Require(scePadSetAngularVelocityDeadbandState(handle, false) == PAD_OK);

    for (int invalidHandle : {-9999, -100, -2, -1, 0, handle + 99, 1000}) {
        Require(scePadResetOrientation(invalidHandle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadband(invalidHandle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadband(invalidHandle, false) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadbandState(invalidHandle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadbandState(invalidHandle, false) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadClose_nid_postfix(handle) == PAD_OK);
    Require(scePadClose_nid_postfix(handle) == PAD_ERROR_INVALID_HANDLE);

    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);

    Require(scePadResetOrientation(special) == PAD_OK);
    Require(scePadSetAngularVelocityDeadband(special, true) == PAD_OK);
    Require(scePadSetAngularVelocityDeadband(special, false) == PAD_OK);
    Require(scePadSetAngularVelocityDeadbandState(special, true) == PAD_OK);
    Require(scePadSetAngularVelocityDeadbandState(special, false) == PAD_OK);

    for (int invalidHandle : {-9999, -100, -2, -1, 0, special + 99, 1000}) {
        Require(scePadResetOrientation(invalidHandle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadband(invalidHandle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadband(invalidHandle, false) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadbandState(invalidHandle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadbandState(invalidHandle, false) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadClose_nid_postfix(special) == PAD_OK);
    Require(scePadClose_nid_postfix(special) == PAD_ERROR_INVALID_HANDLE);

    Require(scePadResetOrientation(special) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetAngularVelocityDeadband(special, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetAngularVelocityDeadband(special, false) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetAngularVelocityDeadbandState(special, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetAngularVelocityDeadbandState(special, false) == PAD_ERROR_INVALID_HANDLE);

    for (int invalidHandle : {-9999, -100, -2, -1, 0, special + 99, 1000}) {
        Require(scePadResetOrientation(invalidHandle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadband(invalidHandle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadband(invalidHandle, false) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadbandState(invalidHandle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetAngularVelocityDeadbandState(invalidHandle, false) == PAD_ERROR_INVALID_HANDLE);
    }

    return 0;
}
