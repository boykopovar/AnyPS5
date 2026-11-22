#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

namespace {

constexpr int PAD_ERROR_DEVICE_NO_HANDLE = static_cast<int>(0x80920008);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "guest_pad_handle_close_bounds: %s\n", message);
    std::abort();
}

void TestPreInitializationBounds() {
    for (int user : {0x10000000, 0x10000001, 0x10000002, 0x10000003}) {
        Require(scePadGetHandle(user, PAD_PORT_TYPE_STANDARD, 0) == PAD_ERROR_DEVICE_NO_HANDLE, "pre-init get handle standard must return DEVICE_NO_HANDLE");
        Require(scePadGetHandle(user, 1, 0) == PAD_ERROR_DEVICE_NO_HANDLE, "pre-init get handle type 1 must return DEVICE_NO_HANDLE");
        Require(scePadGetHandle(user, PAD_PORT_TYPE_SPECIAL, 0) == PAD_ERROR_DEVICE_NO_HANDLE, "pre-init get handle special must return DEVICE_NO_HANDLE");
        Require(scePadGetHandle(user, PAD_PORT_TYPE_REMOTE, 0) == PAD_ERROR_DEVICE_NO_HANDLE, "pre-init get handle remote must return DEVICE_NO_HANDLE");
        Require(scePadGetHandle(user, PAD_PORT_TYPE_STANDARD, 1) == PAD_ERROR_DEVICE_NO_HANDLE, "pre-init get handle index 1 must return DEVICE_NO_HANDLE");
        Require(scePadGetHandle(user, PAD_PORT_TYPE_STANDARD, -1) == PAD_ERROR_DEVICE_NO_HANDLE, "pre-init get handle index -1 must return DEVICE_NO_HANDLE");
    }

    for (int invalidUser : {-1, 0, 0x0fffffff, 0x10000004, 0x20000000}) {
        Require(scePadGetHandle(invalidUser, PAD_PORT_TYPE_STANDARD, 0) == PAD_ERROR_DEVICE_NO_HANDLE, "pre-init invalid user must return DEVICE_NO_HANDLE");
    }

    Require(scePadGetHandle(PAD_USER_ID_SYSTEM, PAD_PORT_TYPE_REMOTE, 0) == PAD_ERROR_DEVICE_NO_HANDLE, "pre-init system remote must return DEVICE_NO_HANDLE");

    for (int handle : {-100, -1, 0, 1, 2, 3, 4, 16, 32, 100, 9999}) {
        Require(scePadClose_nid_postfix(handle) == PAD_ERROR_INVALID_HANDLE, "pre-init close must return INVALID_HANDLE");
    }
}

void TestPostInitializationUnopenedHandleBounds() {
    Require(scePadInit_nid_postfix() == PAD_OK, "scePadInit must return PAD_OK");
    Require(scePadInit_nid_postfix() == PAD_OK, "repeat scePadInit must return PAD_OK");

    for (int user : {0x10000000, 0x10000001, 0x10000002, 0x10000003}) {
        Require(scePadGetHandle(user, PAD_PORT_TYPE_STANDARD, 0) == PAD_ERROR_DEVICE_NO_HANDLE, "unopened standard get handle must return DEVICE_NO_HANDLE");
        Require(scePadGetHandle(user, 1, 0) == PAD_ERROR_DEVICE_NO_HANDLE, "unopened type 1 get handle must return DEVICE_NO_HANDLE");
        Require(scePadGetHandle(user, PAD_PORT_TYPE_SPECIAL, 0) == PAD_ERROR_DEVICE_NO_HANDLE, "unopened special get handle must return DEVICE_NO_HANDLE");
        Require(scePadGetHandle(user, PAD_PORT_TYPE_REMOTE, 0) == PAD_ERROR_DEVICE_NO_HANDLE, "unopened remote get handle must return DEVICE_NO_HANDLE");
        Require(scePadGetHandle(user, PAD_PORT_TYPE_STANDARD, 1) == PAD_ERROR_DEVICE_NO_HANDLE, "index 1 get handle must return DEVICE_NO_HANDLE");
    }

    for (int invalidUser : {-500, -1, 0, 1, 0x000000ff, 0x10000004, 0x10000005, 0x7fffffff}) {
        Require(scePadGetHandle(invalidUser, PAD_PORT_TYPE_STANDARD, 0) == PAD_ERROR_DEVICE_NO_HANDLE, "invalid user get handle must return DEVICE_NO_HANDLE");
    }

    for (int handle : {-500, -1, 0, 1, 2, 3, 4, 16, 32, 47, 500}) {
        Require(scePadClose_nid_postfix(handle) == PAD_ERROR_INVALID_HANDLE, "unopened handle close must return INVALID_HANDLE");
    }
}

void TestStandardUserHandlesAndLifecycle() {
    const int handle0 = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle0 == 1, "open port 0 must return handle 1");

    const int handle1 = scePadOpen_nid_postfix(0x10000001, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle1 == 2, "open port 1 must return handle 2");

    const int handle2 = scePadOpen_nid_postfix(0x10000002, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle2 == 3, "open port 2 must return handle 3");

    const int handle3 = scePadOpen_nid_postfix(0x10000003, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle3 == 4, "open port 3 must return handle 4");

    Require(scePadGetHandle(0x10000000, PAD_PORT_TYPE_STANDARD, 0) == handle0, "get handle 0 failed");
    Require(scePadGetHandle(0x10000001, PAD_PORT_TYPE_STANDARD, 0) == handle1, "get handle 1 failed");
    Require(scePadGetHandle(0x10000002, PAD_PORT_TYPE_STANDARD, 0) == handle2, "get handle 2 failed");
    Require(scePadGetHandle(0x10000003, PAD_PORT_TYPE_STANDARD, 0) == handle3, "get handle 3 failed");

    for (int user : {0x10000000, 0x10000001, 0x10000002, 0x10000003}) {
        Require(scePadGetHandle(user, 1, 0) == PAD_ERROR_DEVICE_NO_HANDLE, "querying port type 1 must return DEVICE_NO_HANDLE");
        Require(scePadGetHandle(user, PAD_PORT_TYPE_SPECIAL, 0) == PAD_ERROR_DEVICE_NO_HANDLE, "querying unopened special port must return DEVICE_NO_HANDLE");
        Require(scePadGetHandle(user, PAD_PORT_TYPE_REMOTE, 0) == PAD_ERROR_DEVICE_NO_HANDLE, "querying non-system remote must return DEVICE_NO_HANDLE");
        Require(scePadGetHandle(user, PAD_PORT_TYPE_STANDARD, 1) == PAD_ERROR_DEVICE_NO_HANDLE, "non-zero index must return DEVICE_NO_HANDLE");
        Require(scePadGetHandle(user, PAD_PORT_TYPE_STANDARD, 2) == PAD_ERROR_DEVICE_NO_HANDLE, "index 2 must return DEVICE_NO_HANDLE");
        Require(scePadGetHandle(user, PAD_PORT_TYPE_STANDARD, -1) == PAD_ERROR_DEVICE_NO_HANDLE, "negative index must return DEVICE_NO_HANDLE");
    }

    Require(scePadClose_nid_postfix(handle1) == PAD_OK, "close handle 1 failed");
    Require(scePadGetHandle(0x10000001, PAD_PORT_TYPE_STANDARD, 0) == PAD_ERROR_DEVICE_NO_HANDLE, "closed user 1 must return DEVICE_NO_HANDLE");

    Require(scePadGetHandle(0x10000000, PAD_PORT_TYPE_STANDARD, 0) == handle0, "handle 0 must remain open");
    Require(scePadGetHandle(0x10000002, PAD_PORT_TYPE_STANDARD, 0) == handle2, "handle 2 must remain open");
    Require(scePadGetHandle(0x10000003, PAD_PORT_TYPE_STANDARD, 0) == handle3, "handle 3 must remain open");

    Require(scePadClose_nid_postfix(handle1) == PAD_OK, "double close on standard handle must return PAD_OK");
    Require(scePadClose_nid_postfix(handle1) == PAD_OK, "triple close on standard handle must return PAD_OK");
    Require(scePadGetHandle(0x10000001, PAD_PORT_TYPE_STANDARD, 0) == PAD_ERROR_DEVICE_NO_HANDLE, "closed user 1 remains DEVICE_NO_HANDLE");

    Require(scePadClose_nid_postfix(handle0) == PAD_OK, "close handle 0 failed");
    Require(scePadClose_nid_postfix(handle2) == PAD_OK, "close handle 2 failed");
    Require(scePadClose_nid_postfix(handle3) == PAD_OK, "close handle 3 failed");

    Require(scePadClose_nid_postfix(handle0) == PAD_OK, "double close on handle 0 must return PAD_OK");
    Require(scePadClose_nid_postfix(handle2) == PAD_OK, "double close on handle 2 must return PAD_OK");
    Require(scePadClose_nid_postfix(handle3) == PAD_OK, "double close on handle 3 must return PAD_OK");

    for (int user : {0x10000000, 0x10000001, 0x10000002, 0x10000003}) {
        Require(scePadGetHandle(user, PAD_PORT_TYPE_STANDARD, 0) == PAD_ERROR_DEVICE_NO_HANDLE, "all closed standard handles must return DEVICE_NO_HANDLE");
    }
}

void TestSpecialHandlesAndLifecycle() {
    const int special0 = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special0 >= PAD_SPECIAL_HANDLE, "open special 0 failed");

    const int special1 = scePadOpen_nid_postfix(0x10000001, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special1 >= PAD_SPECIAL_HANDLE, "open special 1 failed");
    Require(special0 != special1, "special handles must be distinct per user");

    Require(scePadGetHandle(0x10000000, PAD_PORT_TYPE_SPECIAL, 0) == special0, "get special 0 handle failed");
    Require(scePadGetHandle(0x10000001, PAD_PORT_TYPE_SPECIAL, 0) == special1, "get special 1 handle failed");
    Require(scePadGetHandle(0x10000002, PAD_PORT_TYPE_SPECIAL, 0) == PAD_ERROR_DEVICE_NO_HANDLE, "unopened special 2 must return DEVICE_NO_HANDLE");

    Require(scePadGetHandle(0x10000000, PAD_PORT_TYPE_SPECIAL, 1) == PAD_ERROR_DEVICE_NO_HANDLE, "special non-zero index must return DEVICE_NO_HANDLE");
    Require(scePadGetHandle(0x10000000, PAD_PORT_TYPE_STANDARD, 0) == PAD_ERROR_DEVICE_NO_HANDLE, "standard query when only special open must return DEVICE_NO_HANDLE");

    Require(scePadClose_nid_postfix(special0) == PAD_OK, "close special 0 failed");
    Require(scePadGetHandle(0x10000000, PAD_PORT_TYPE_SPECIAL, 0) == PAD_ERROR_DEVICE_NO_HANDLE, "closed special 0 must return DEVICE_NO_HANDLE");

    Require(scePadGetHandle(0x10000001, PAD_PORT_TYPE_SPECIAL, 0) == special1, "special 1 must remain open");

    Require(scePadClose_nid_postfix(special0) == PAD_ERROR_INVALID_HANDLE, "double close on special handle must return INVALID_HANDLE");
    Require(scePadClose_nid_postfix(special0) == PAD_ERROR_INVALID_HANDLE, "triple close on special handle must return INVALID_HANDLE");

    Require(scePadClose_nid_postfix(special1) == PAD_OK, "close special 1 failed");
    Require(scePadClose_nid_postfix(special1) == PAD_ERROR_INVALID_HANDLE, "double close on special 1 must return INVALID_HANDLE");
    Require(scePadGetHandle(0x10000001, PAD_PORT_TYPE_SPECIAL, 0) == PAD_ERROR_DEVICE_NO_HANDLE, "closed special 1 must return DEVICE_NO_HANDLE");
}

void TestRemoteHandleAndLifecycle() {
    const int remoteHandle = scePadOpen_nid_postfix(PAD_USER_ID_SYSTEM, PAD_PORT_TYPE_REMOTE, 0, nullptr);
    Require(remoteHandle == PAD_REMOTE_HANDLE, "open remote port must return PAD_REMOTE_HANDLE");

    Require(scePadGetHandle(PAD_USER_ID_SYSTEM, PAD_PORT_TYPE_REMOTE, 0) == PAD_REMOTE_HANDLE, "get remote handle failed");
    Require(scePadGetHandle(0x10000000, PAD_PORT_TYPE_REMOTE, 0) == PAD_ERROR_DEVICE_NO_HANDLE, "non-system remote get handle must return DEVICE_NO_HANDLE");
    Require(scePadGetHandle(PAD_USER_ID_SYSTEM, PAD_PORT_TYPE_REMOTE, 1) == PAD_ERROR_DEVICE_NO_HANDLE, "remote non-zero index must return DEVICE_NO_HANDLE");
    Require(scePadGetHandle(PAD_USER_ID_SYSTEM, PAD_PORT_TYPE_STANDARD, 0) == PAD_ERROR_DEVICE_NO_HANDLE, "system standard get handle must return DEVICE_NO_HANDLE");

    Require(scePadClose_nid_postfix(remoteHandle) == PAD_OK, "close remote handle failed");
    Require(scePadGetHandle(PAD_USER_ID_SYSTEM, PAD_PORT_TYPE_REMOTE, 0) == PAD_ERROR_DEVICE_NO_HANDLE, "closed remote handle query must return DEVICE_NO_HANDLE");

    Require(scePadClose_nid_postfix(remoteHandle) == PAD_ERROR_INVALID_HANDLE, "double close on remote handle must return INVALID_HANDLE");
    Require(scePadClose_nid_postfix(remoteHandle) == PAD_ERROR_INVALID_HANDLE, "triple close on remote handle must return INVALID_HANDLE");
}

void TestClosedHandleErrorPrecedenceOnOtherPadCalls() {
    const int handle0 = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle0 == 1, "re-open port 0 failed");

    Require(scePadIsValidHandle(handle0, PAD_PORT_TYPE_STANDARD), "open handle0 must be valid");
    Require(!scePadIsValidHandle(handle0, PAD_PORT_TYPE_SPECIAL), "handle0 must not be valid special");
    Require(!scePadIsValidHandle(handle0, PAD_PORT_TYPE_REMOTE), "handle0 must not be valid remote");

    Require(scePadClose_nid_postfix(handle0) == PAD_OK, "close handle0 failed");

    Require(!scePadIsValidHandle(handle0, PAD_PORT_TYPE_STANDARD), "closed standard handle must not be valid");
    Require(!scePadIsValidHandle(handle0, PAD_PORT_TYPE_SPECIAL), "closed standard handle must not be valid special");

    Require(!scePadIsValidHandle(10, PAD_PORT_TYPE_STANDARD), "unopened handle 10 must not be valid");
    Require(!scePadIsValidHandle(-1, PAD_PORT_TYPE_STANDARD), "handle -1 must not be valid");
    Require(!scePadIsValidHandle(0, PAD_PORT_TYPE_STANDARD), "handle 0 must not be valid");

    Require(scePadGetHandle(0x10000000, PAD_PORT_TYPE_STANDARD, 0) == PAD_ERROR_DEVICE_NO_HANDLE, "get handle for closed port 0 must return DEVICE_NO_HANDLE");
}

} // namespace

int main() {
    TestPreInitializationBounds();
    TestPostInitializationUnopenedHandleBounds();
    TestStandardUserHandlesAndLifecycle();
    TestSpecialHandlesAndLifecycle();
    TestRemoteHandleAndLifecycle();
    TestClosedHandleErrorPrecedenceOnOtherPadCalls();
    return 0;
}
