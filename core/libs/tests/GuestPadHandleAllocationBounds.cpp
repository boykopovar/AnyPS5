#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <source_location>

namespace {

constexpr int PAD_ERROR_DEVICE_NO_HANDLE = static_cast<int>(0x80920008u);
constexpr int PAD_ERROR_DEVICE_NOT_CONNECTED = static_cast<int>(0x80920007u);
constexpr int PAD_ERROR_ALREADY_OPENED = static_cast<int>(0x80920004u);
constexpr int PAD_ERROR_UNKNOWN_USER = static_cast<int>(0x809b0081u);
constexpr int PAD_ERROR_INVALID_PORT_INDEX = static_cast<int>(0x809b0003u);
constexpr int PAD_ERROR_INVALID_USER_ID = static_cast<int>(0x809b0001u);
constexpr int PAD_USER_ID_BASE = 0x10000000;
constexpr int PAD_USER_ID_INVALID = -1;

void Require(bool condition, const std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "guest_pad_handle_allocation_bounds failed at line %u\n", static_cast<unsigned>(location.line()));
        std::fflush(stderr);
        std::abort();
    }
}

void TestPreOpenAndParameterRejection() {
    for (int port = 0; port < PAD_MAX_CONTROLLERS; ++port) {
        const int user = PAD_USER_ID_BASE + port;
        Require(scePadGetHandle(user, PAD_PORT_TYPE_STANDARD, 0) == PAD_ERROR_DEVICE_NO_HANDLE);
        Require(scePadGetHandle(user, PAD_PORT_TYPE_SPECIAL, 0) == PAD_ERROR_DEVICE_NO_HANDLE);
    }

    for (int invalidUser : {-100, -1, 0, 1, 0x0FFFFFFF, 0x10000004, 0x10000005, 0x20000000, 0x7FFFFFFF}) {
        Require(scePadGetHandle(invalidUser, PAD_PORT_TYPE_STANDARD, 0) == PAD_ERROR_DEVICE_NO_HANDLE);
    }

    for (int port = 0; port < PAD_MAX_CONTROLLERS; ++port) {
        const int user = PAD_USER_ID_BASE + port;
        for (int invalidIndex : {-10, -1, 1, 2, 3, 10, 255}) {
            Require(scePadGetHandle(user, PAD_PORT_TYPE_STANDARD, invalidIndex) == PAD_ERROR_DEVICE_NO_HANDLE);
            Require(scePadGetHandle(user, PAD_PORT_TYPE_SPECIAL, invalidIndex) == PAD_ERROR_DEVICE_NO_HANDLE);
        }
    }

    for (int invalidType : {-1, 1, 3, 4, 5, 6, 7, 15, 17, 255}) {
        Require(scePadGetHandle(PAD_USER_ID_BASE, invalidType, 0) == PAD_ERROR_DEVICE_NO_HANDLE);
    }

    Require(scePadGetHandle(PAD_USER_ID_SYSTEM, PAD_PORT_TYPE_REMOTE, 0) == PAD_ERROR_DEVICE_NO_HANDLE);
    Require(scePadGetHandle(PAD_USER_ID_BASE, PAD_PORT_TYPE_REMOTE, 0) == PAD_ERROR_DEVICE_NO_HANDLE);
}

void TestMultiControllerLifecycle() {
    std::array<int, PAD_MAX_CONTROLLERS> handles{};

    for (int port = 0; port < PAD_MAX_CONTROLLERS; ++port) {
        const int user = PAD_USER_ID_BASE + port;
        const int handle = scePadOpen_nid_postfix(user, PAD_PORT_TYPE_STANDARD, 0, nullptr);
        Require(handle == PAD_HANDLE_BASE + port);
        Require(scePadGetHandle(user, PAD_PORT_TYPE_STANDARD, 0) == handle);
        Require(scePadIsValidHandle(handle, PAD_PORT_TYPE_STANDARD));
        Require(!scePadIsValidHandle(handle, PAD_PORT_TYPE_SPECIAL));
        Require(!scePadIsValidHandle(handle, PAD_PORT_TYPE_REMOTE));
        Require(!scePadIsValidHandle(handle, 1));
        handles[port] = handle;
    }

    Require(scePadOpen_nid_postfix(PAD_USER_ID_BASE + 4, PAD_PORT_TYPE_STANDARD, 0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetHandle(PAD_USER_ID_BASE + 4, PAD_PORT_TYPE_STANDARD, 0) == PAD_ERROR_DEVICE_NO_HANDLE);

    PadInfo info{};
    Require(scePadGetInfo_nid_postfix(&info) == PAD_OK);
    Require(info.maxConnectCount == PAD_MAX_CONTROLLERS);

    Require(scePadClose_nid_postfix(handles[1]) == PAD_OK);
    Require(scePadGetHandle(PAD_USER_ID_BASE + 1, PAD_PORT_TYPE_STANDARD, 0) == PAD_ERROR_DEVICE_NO_HANDLE);
    Require(!scePadIsValidHandle(handles[1], PAD_PORT_TYPE_STANDARD));

    Require(scePadGetHandle(PAD_USER_ID_BASE + 0, PAD_PORT_TYPE_STANDARD, 0) == handles[0]);
    Require(scePadGetHandle(PAD_USER_ID_BASE + 2, PAD_PORT_TYPE_STANDARD, 0) == handles[2]);
    Require(scePadGetHandle(PAD_USER_ID_BASE + 3, PAD_PORT_TYPE_STANDARD, 0) == handles[3]);

    const int reopened = scePadOpen_nid_postfix(PAD_USER_ID_BASE + 1, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(reopened == handles[1]);
    Require(scePadGetHandle(PAD_USER_ID_BASE + 1, PAD_PORT_TYPE_STANDARD, 0) == handles[1]);
    Require(scePadIsValidHandle(reopened, PAD_PORT_TYPE_STANDARD));

    for (int port = 0; port < PAD_MAX_CONTROLLERS; ++port) {
        Require(scePadClose_nid_postfix(handles[port]) == PAD_OK);
        Require(scePadGetHandle(PAD_USER_ID_BASE + port, PAD_PORT_TYPE_STANDARD, 0) == PAD_ERROR_DEVICE_NO_HANDLE);
        Require(!scePadIsValidHandle(handles[port], PAD_PORT_TYPE_STANDARD));
    }
}

void TestSpecialPortLifecycleAndLimits() {
    std::array<int, PAD_SPECIAL_HANDLE_COUNT> specials{};

    for (int slot = 0; slot < PAD_SPECIAL_HANDLE_COUNT; ++slot) {
        const int user = 0x20000000 + slot;
        const int handle = scePadOpen_nid_postfix(user, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
        Require(handle == PAD_SPECIAL_HANDLE + slot);
        Require(scePadGetHandle(user, PAD_PORT_TYPE_SPECIAL, 0) == handle);
        Require(scePadIsValidHandle(handle, PAD_PORT_TYPE_SPECIAL));
        Require(!scePadIsValidHandle(handle, PAD_PORT_TYPE_STANDARD));
        Require(!scePadIsValidHandle(handle, PAD_PORT_TYPE_REMOTE));
        specials[slot] = handle;
    }

    Require(scePadOpen_nid_postfix(0x20000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr) == PAD_ERROR_ALREADY_OPENED);

    const int excessUser = 0x20000000 + PAD_SPECIAL_HANDLE_COUNT;
    Require(scePadOpen_nid_postfix(excessUser, PAD_PORT_TYPE_SPECIAL, 0, nullptr) == PAD_ERROR_UNKNOWN_USER);
    Require(scePadGetHandle(excessUser, PAD_PORT_TYPE_SPECIAL, 0) == PAD_ERROR_DEVICE_NO_HANDLE);

    constexpr int freeSlot = 6;
    Require(scePadClose_nid_postfix(specials[freeSlot]) == PAD_OK);
    Require(scePadGetHandle(0x20000000 + freeSlot, PAD_PORT_TYPE_SPECIAL, 0) == PAD_ERROR_DEVICE_NO_HANDLE);
    Require(!scePadIsValidHandle(specials[freeSlot], PAD_PORT_TYPE_SPECIAL));

    const int recycled = scePadOpen_nid_postfix(excessUser, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(recycled == PAD_SPECIAL_HANDLE + freeSlot);
    Require(scePadGetHandle(excessUser, PAD_PORT_TYPE_SPECIAL, 0) == recycled);
    Require(scePadIsValidHandle(recycled, PAD_PORT_TYPE_SPECIAL));

    for (int slot = 0; slot < PAD_SPECIAL_HANDLE_COUNT; ++slot) {
        if (slot == freeSlot) {
            Require(scePadClose_nid_postfix(recycled) == PAD_OK);
            Require(scePadGetHandle(excessUser, PAD_PORT_TYPE_SPECIAL, 0) == PAD_ERROR_DEVICE_NO_HANDLE);
        } else {
            Require(scePadClose_nid_postfix(specials[slot]) == PAD_OK);
            Require(scePadGetHandle(0x20000000 + slot, PAD_PORT_TYPE_SPECIAL, 0) == PAD_ERROR_DEVICE_NO_HANDLE);
        }
    }
}

void TestRemoteControlLifecycle() {
    Require(scePadGetHandle(PAD_USER_ID_SYSTEM, PAD_PORT_TYPE_REMOTE, 0) == PAD_ERROR_DEVICE_NO_HANDLE);

    for (int invalidType : {PAD_PORT_TYPE_STANDARD, PAD_PORT_TYPE_SPECIAL, 1, 3, 4}) {
        Require(scePadGetHandle(PAD_USER_ID_SYSTEM, invalidType, 0) == PAD_ERROR_DEVICE_NO_HANDLE);
    }

    Require(scePadOpen_nid_postfix(PAD_USER_ID_BASE, PAD_PORT_TYPE_REMOTE, 0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetHandle(PAD_USER_ID_BASE, PAD_PORT_TYPE_REMOTE, 0) == PAD_ERROR_DEVICE_NO_HANDLE);

    const int remote = scePadOpen_nid_postfix(PAD_USER_ID_SYSTEM, PAD_PORT_TYPE_REMOTE, 0, nullptr);
    Require(remote == PAD_REMOTE_HANDLE);
    Require(scePadGetHandle(PAD_USER_ID_SYSTEM, PAD_PORT_TYPE_REMOTE, 0) == remote);
    Require(scePadGetHandle(PAD_USER_ID_SYSTEM, PAD_PORT_TYPE_REMOTE, 1) == PAD_ERROR_DEVICE_NO_HANDLE);
    Require(scePadIsValidHandle(remote, PAD_PORT_TYPE_REMOTE));
    Require(!scePadIsValidHandle(remote, PAD_PORT_TYPE_STANDARD));
    Require(!scePadIsValidHandle(remote, PAD_PORT_TYPE_SPECIAL));

    Require(scePadClose_nid_postfix(remote) == PAD_OK);
    Require(scePadGetHandle(PAD_USER_ID_SYSTEM, PAD_PORT_TYPE_REMOTE, 0) == PAD_ERROR_DEVICE_NO_HANDLE);
    Require(!scePadIsValidHandle(remote, PAD_PORT_TYPE_REMOTE));
}

void TestOpenValidationPrecedence() {
    Require(scePadOpen_nid_postfix(PAD_USER_ID_INVALID, PAD_PORT_TYPE_STANDARD, 0, nullptr) == PAD_ERROR_INVALID_USER_ID);
    Require(scePadOpen_nid_postfix(PAD_USER_ID_INVALID, 3, 0, nullptr) == PAD_ERROR_INVALID_USER_ID);
    Require(scePadOpen_nid_postfix(PAD_USER_ID_INVALID, 4, 0, nullptr) == PAD_ERROR_INVALID_USER_ID);
    Require(scePadOpen_nid_postfix(PAD_USER_ID_INVALID, 1, 0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadOpen_nid_postfix(PAD_USER_ID_INVALID, 5, 0, nullptr) == PAD_ERROR_INVALID_ARG);

    for (int unknownType : {3, 4}) {
        Require(scePadOpen_nid_postfix(PAD_USER_ID_BASE, unknownType, 0, nullptr) == PAD_ERROR_INVALID_ARG);
    }

    Require(scePadOpen_nid_postfix(PAD_USER_ID_BASE, PAD_PORT_TYPE_STANDARD, 1, nullptr) == PAD_ERROR_INVALID_PORT_INDEX);
    Require(scePadOpen_nid_postfix(PAD_USER_ID_BASE, PAD_PORT_TYPE_STANDARD, -1, nullptr) == PAD_ERROR_INVALID_PORT_INDEX);

    std::array<std::uint8_t, 16> param{};
    Require(scePadOpenExt(PAD_USER_ID_BASE, PAD_PORT_TYPE_STANDARD, 1, param.data()) == PAD_ERROR_INVALID_ARG);
    Require(scePadOpenExt(PAD_USER_ID_BASE, PAD_PORT_TYPE_STANDARD, 0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadOpenExt(PAD_USER_ID_BASE, PAD_PORT_TYPE_SPECIAL, 0, param.data()) == PAD_ERROR_DEVICE_NOT_CONNECTED);
}

}

int main() {
    Require(!scePadIsValidHandle(1, PAD_PORT_TYPE_STANDARD));
    Require(scePadInit_nid_postfix() == PAD_OK);

    TestPreOpenAndParameterRejection();
    TestMultiControllerLifecycle();
    TestSpecialPortLifecycleAndLimits();
    TestRemoteControlLifecycle();
    TestOpenValidationPrecedence();

    return 0;
}
