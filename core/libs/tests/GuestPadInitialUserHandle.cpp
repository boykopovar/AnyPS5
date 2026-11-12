#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/LaterExports.hpp"
#include <cstdlib>
#include <stdexcept>
#include <string_view>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestUninitializedState() {
    int busType = -1;
    int jack = -1;
    PadContainerIdInformation container{};

    Require(scePadGetParticularMode() == PAD_ERROR_NOT_INITIALIZED);
    Require(!scePadIsValidHandle(1, PAD_PORT_TYPE_STANDARD));
    Require(!scePadIsValidHandle(0, PAD_PORT_TYPE_STANDARD));
    Require(!scePadIsValidHandle(-1, PAD_PORT_TYPE_STANDARD));
    Require(scePadGetControllerBusType(1, &busType) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadGetControllerBusType(1, nullptr) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadGetJackState(1, &jack) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadGetJackState(1, nullptr) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadGetContainerIdInformation(1, &container) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadGetContainerIdInformation(1, nullptr) == PAD_ERROR_NOT_INITIALIZED);
}

void TestUserHandleLifecycleAndBounds() {
    constexpr int initialUserId = 0x10000000;
    constexpr int secondUserId = 0x10000001;
    constexpr int invalidUserId = -1;
    constexpr int outOfRangeUserId = 0x10000000 + PAD_MAX_CONTROLLERS;
    constexpr int invalidPortIndex = static_cast<int>(0x809b0003);
    constexpr int invalidUserIdError = static_cast<int>(0x809b0001);
    constexpr int deviceNoHandle = static_cast<int>(0x80920008);

    int busType = -1;
    int jack = -1;
    PadContainerIdInformation container{};
    PadVibrationParam vibration{};

    Require(scePadOpen_nid_postfix(invalidUserId, PAD_PORT_TYPE_STANDARD, 0, nullptr) == invalidUserIdError);
    Require(scePadOpen_nid_postfix(outOfRangeUserId, PAD_PORT_TYPE_STANDARD, 0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadOpen_nid_postfix(initialUserId, PAD_PORT_TYPE_STANDARD, 1, nullptr) == invalidPortIndex);
    Require(scePadOpen_nid_postfix(initialUserId, PAD_PORT_TYPE_STANDARD, -1, nullptr) == invalidPortIndex);
    Require(scePadOpen_nid_postfix(initialUserId, 5, 0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadOpen_nid_postfix(initialUserId, -1, 0, nullptr) == PAD_ERROR_INVALID_ARG);

    Require(scePadGetHandle(initialUserId, PAD_PORT_TYPE_STANDARD, 0) == deviceNoHandle);
    Require(scePadGetHandle(secondUserId, PAD_PORT_TYPE_STANDARD, 0) == deviceNoHandle);
    Require(scePadGetHandle(initialUserId, PAD_PORT_TYPE_STANDARD, 1) == deviceNoHandle);
    Require(scePadGetHandle(invalidUserId, PAD_PORT_TYPE_STANDARD, 0) == deviceNoHandle);

    for (int invalidHandle : {-1, 0, 5, 100, 9999}) {
        Require(!scePadIsValidHandle(invalidHandle, PAD_PORT_TYPE_STANDARD));
        Require(scePadClose_nid_postfix(invalidHandle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadGetControllerBusType(invalidHandle, nullptr) == PAD_ERROR_INVALID_ARG);
        Require(scePadGetJackState(invalidHandle, nullptr) == PAD_ERROR_INVALID_ARG);
        Require(scePadGetContainerIdInformation(invalidHandle, nullptr) == PAD_ERROR_INVALID_ARG);
        Require(scePadSetVibration(invalidHandle, nullptr) == PAD_ERROR_INVALID_ARG);
        Require(scePadGetControllerBusType(invalidHandle, &busType) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadGetJackState(invalidHandle, &jack) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadGetContainerIdInformation(invalidHandle, &container) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetVibration(invalidHandle, &vibration) == PAD_ERROR_INVALID_HANDLE);
    }

    const int initialHandle = scePadOpen_nid_postfix(initialUserId, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(initialHandle == PAD_HANDLE_BASE);
    Require(scePadGetHandle(initialUserId, PAD_PORT_TYPE_STANDARD, 0) == initialHandle);
    Require(scePadIsValidHandle(initialHandle, PAD_PORT_TYPE_STANDARD));
    Require(!scePadIsValidHandle(initialHandle, PAD_PORT_TYPE_SPECIAL));
    Require(!scePadIsValidHandle(initialHandle, PAD_PORT_TYPE_REMOTE));

    Require(scePadGetControllerBusType(initialHandle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetJackState(initialHandle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetContainerIdInformation(initialHandle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetVibration(initialHandle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetControllerBusType(initialHandle, &busType) == PAD_OK);
    Require(scePadGetJackState(initialHandle, &jack) == PAD_OK);
    Require(scePadGetContainerIdInformation(initialHandle, &container) == PAD_OK);

    const int secondHandle = scePadOpen_nid_postfix(secondUserId, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(secondHandle == PAD_HANDLE_BASE + 1);
    Require(scePadGetHandle(secondUserId, PAD_PORT_TYPE_STANDARD, 0) == secondHandle);
    Require(scePadIsValidHandle(secondHandle, PAD_PORT_TYPE_STANDARD));

    Require(scePadClose_nid_postfix(secondHandle) == PAD_OK);
    Require(!scePadIsValidHandle(secondHandle, PAD_PORT_TYPE_STANDARD));
    Require(scePadGetHandle(secondUserId, PAD_PORT_TYPE_STANDARD, 0) == deviceNoHandle);
    Require(scePadClose_nid_postfix(secondHandle) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadGetControllerBusType(secondHandle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetControllerBusType(secondHandle, &busType) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadGetJackState(secondHandle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetJackState(secondHandle, &jack) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadGetContainerIdInformation(secondHandle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetContainerIdInformation(secondHandle, &container) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetVibration(secondHandle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetVibration(secondHandle, &vibration) == PAD_ERROR_INVALID_HANDLE);

    const int specialHandle = scePadOpen_nid_postfix(initialUserId, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(specialHandle >= PAD_SPECIAL_HANDLE);
    Require(scePadIsValidHandle(specialHandle, PAD_PORT_TYPE_SPECIAL));
    Require(!scePadIsValidHandle(specialHandle, PAD_PORT_TYPE_STANDARD));
    Require(scePadGetHandle(initialUserId, PAD_PORT_TYPE_SPECIAL, 0) == specialHandle);
    Require(scePadClose_nid_postfix(specialHandle) == PAD_OK);
    Require(!scePadIsValidHandle(specialHandle, PAD_PORT_TYPE_SPECIAL));

    Require(scePadClose_nid_postfix(initialHandle) == PAD_OK);
    Require(!scePadIsValidHandle(initialHandle, PAD_PORT_TYPE_STANDARD));
    Require(scePadClose_nid_postfix(initialHandle) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadGetControllerBusType(initialHandle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetControllerBusType(initialHandle, &busType) == PAD_ERROR_INVALID_HANDLE);
}

void TestLaterExportsIsInitialUserHandlePlaceholder() {
    bool caught = false;
    try {
        scePadIsInitialUserHandle();
    } catch (const std::runtime_error& error) {
        caught = std::string_view(error.what()).find("scePadIsInitialUserHandle") != std::string_view::npos;
    }
    Require(caught);
}

} // namespace

int main() {
    TestUninitializedState();
    Require(scePadInit_nid_postfix() == PAD_OK);
    TestUserHandleLifecycleAndBounds();
    TestLaterExportsIsInitialUserHandlePlaceholder();
    return 0;
}
