#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <cstdio>
#include <source_location>

namespace {

constexpr int PAD_ERROR_DEVICE_NOT_CONNECTED = static_cast<int>(0x80920007);

void Require(bool condition, std::source_location loc = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Require failed at line %u\n", loc.line());
        std::abort();
    }
}

void TestPortIndexLimits() {
    constexpr int initialUserId = 0x10000000;
    constexpr int invalidUserId = -1;
    constexpr int outOfRangeUserId = 0x10000000 + PAD_MAX_CONTROLLERS;
    constexpr int invalidPortIndex = static_cast<int>(0x809b0003);
    constexpr int invalidUserIdError = static_cast<int>(0x809b0001);
    constexpr int deviceNoHandle = static_cast<int>(0x80920008);

    Require(scePadOpen_nid_postfix(invalidUserId, PAD_PORT_TYPE_STANDARD, 0, nullptr) == invalidUserIdError);
    Require(scePadOpen_nid_postfix(outOfRangeUserId, PAD_PORT_TYPE_STANDARD, 0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadOpen_nid_postfix(initialUserId, PAD_PORT_TYPE_STANDARD, 1, nullptr) == invalidPortIndex);
    Require(scePadOpen_nid_postfix(initialUserId, PAD_PORT_TYPE_STANDARD, 2, nullptr) == invalidPortIndex);
    Require(scePadOpen_nid_postfix(initialUserId, PAD_PORT_TYPE_STANDARD, -1, nullptr) == invalidPortIndex);
    Require(scePadOpen_nid_postfix(initialUserId, PAD_PORT_TYPE_STANDARD, 64, nullptr) == invalidPortIndex);

    Require(scePadGetHandle(initialUserId, PAD_PORT_TYPE_STANDARD, 1) == deviceNoHandle);
    Require(scePadGetHandle(initialUserId, PAD_PORT_TYPE_STANDARD, 2) == deviceNoHandle);
    Require(scePadGetHandle(initialUserId, PAD_PORT_TYPE_STANDARD, -1) == deviceNoHandle);
    Require(scePadGetHandle(initialUserId, PAD_PORT_TYPE_STANDARD, 64) == deviceNoHandle);
}

void TestDescriptorBounds() {
    constexpr int invalidHandleError = static_cast<int>(0x80920003);
    PadVibrationParam vibration{};

    for (int invalidHandle : {-9999, -1, 0, 5, 15, 32, 100, 9999}) {
        Require(scePadClose_nid_postfix(invalidHandle) == invalidHandleError);
        Require(scePadSetVibrationForce(invalidHandle, &vibration) == PAD_ERROR_NOT_INITIALIZED);
    }

    Require(scePadInit_nid_postfix() == PAD_OK);

    for (int invalidHandle : {-9999, -1, 0, 5, 15, 32, 100, 9999}) {
        Require(scePadClose_nid_postfix(invalidHandle) == invalidHandleError);
        Require(scePadSetVibrationForce(invalidHandle, &vibration) == invalidHandleError);
    }
}

void TestTransactionalOutputPointerSafety() {

    const int handle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle > 0);

#ifdef _WIN32
    SYSTEM_INFO sysInfo{};
    GetSystemInfo(&sysInfo);
    void* readOnlyPage = VirtualAlloc(nullptr, sysInfo.dwPageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READONLY);
    Require(readOnlyPage != nullptr);
#else
    const long pageSize = sysconf(_SC_PAGESIZE);
    Require(pageSize > 0);
    void* readOnlyPage = mmap(nullptr, static_cast<std::size_t>(pageSize), PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(readOnlyPage != MAP_FAILED);
#endif

    errno = 71;
    Require(scePadRead_nid_postfix(handle, reinterpret_cast<PadData*>(0x10), 1) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);

    Require(scePadRead_nid_postfix(handle, reinterpret_cast<PadData*>(0x10), 4) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);

    Require(scePadRead_nid_postfix(handle, static_cast<PadData*>(readOnlyPage), 1) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);

    PadData validData{};
    Require(scePadRead_nid_postfix(handle, &validData, 1) == 1);

    Require(scePadGetControllerInformation(handle, reinterpret_cast<PadControllerInformation*>(0x10)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);
    Require(scePadGetControllerInformation(handle, static_cast<PadControllerInformation*>(readOnlyPage)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);

    Require(scePadGetInfo_nid_postfix(reinterpret_cast<PadInfo*>(0x10)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);
    Require(scePadGetInfo_nid_postfix(static_cast<PadInfo*>(readOnlyPage)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);

    Require(scePadGetTriggerEffectState(handle, reinterpret_cast<PadTriggerEffectStateInformation*>(0x10)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);
    Require(scePadGetTriggerEffectState(handle, static_cast<PadTriggerEffectStateInformation*>(readOnlyPage)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);

    Require(scePadGetControllerBusType(handle, reinterpret_cast<int*>(0x10)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);
    Require(scePadGetControllerBusType(handle, static_cast<int*>(readOnlyPage)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);

    Require(scePadGetJackState(handle, reinterpret_cast<int*>(0x10)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);
    Require(scePadGetJackState(handle, static_cast<int*>(readOnlyPage)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);

    Require(scePadGetContainerIdInformation(handle, reinterpret_cast<PadContainerIdInformation*>(0x10)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);
    Require(scePadGetContainerIdInformation(handle, static_cast<PadContainerIdInformation*>(readOnlyPage)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);

    Require(scePadIsRemoteController(handle, reinterpret_cast<bool*>(0x10)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);
    Require(scePadIsRemoteController(handle, static_cast<bool*>(readOnlyPage)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);

    Require(scePadGetAccessibilityControllerState(handle, reinterpret_cast<std::uint32_t*>(0x10)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);
    Require(scePadGetAccessibilityControllerState(handle, static_cast<std::uint32_t*>(readOnlyPage)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);

    Require(scePadGetControllerType(handle, reinterpret_cast<std::int32_t*>(0x10)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);
    Require(scePadGetControllerType(handle, static_cast<std::int32_t*>(readOnlyPage)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);

    Require(scePadDeviceClassGetExtendedInformation(handle, reinterpret_cast<PadDeviceClassExtendedInformation*>(0x10)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);
    Require(scePadDeviceClassGetExtendedInformation(handle, static_cast<PadDeviceClassExtendedInformation*>(readOnlyPage)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);

    Require(scePadDeviceClassParseData(handle, &validData, reinterpret_cast<PadDeviceClassData*>(0x10)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);
    Require(scePadDeviceClassParseData(handle, &validData, static_cast<PadDeviceClassData*>(readOnlyPage)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);

#ifdef _WIN32
    Require(VirtualFree(readOnlyPage, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(readOnlyPage, static_cast<std::size_t>(pageSize)) == 0);
#endif

#ifdef _WIN32
    void* watchedPage = VirtualAlloc(nullptr, sysInfo.dwPageSize, MEM_RESERVE | MEM_COMMIT | MEM_WRITE_WATCH, PAGE_READWRITE);
    Require(watchedPage != nullptr);
#else
    void* watchedPage = mmap(nullptr, static_cast<std::size_t>(pageSize), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(watchedPage != MAP_FAILED);
    if (GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) {
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(watchedPage, static_cast<std::size_t>(pageSize));
    }
#endif

    PadControllerInformation validInfo{};
    Require(scePadGetControllerInformation(handle, &validInfo) == PAD_OK);

    PadData* watchedData = static_cast<PadData*>(watchedPage);
    Require(scePadRead_nid_postfix(handle, watchedData, 1) == 1);
    Require(watchedData->connected == validData.connected && watchedData->timestamp >= validData.timestamp);

    PadControllerInformation* watchedInfo = static_cast<PadControllerInformation*>(watchedPage);
    Require(scePadGetControllerInformation(handle, watchedInfo) == PAD_OK);
    Require(std::memcmp(watchedInfo, &validInfo, sizeof(validInfo)) == 0);

    auto* watchedMotion = static_cast<PadMotionSensorPosition*>(watchedPage);
    Require(scePadGetMotionSensorPosition(handle, watchedMotion) == PAD_ERROR_DEVICE_NOT_CONNECTED);

    auto* watchedAddress = static_cast<std::uint64_t*>(watchedPage);
    Require(scePadGetBluetoothAddress(handle, watchedAddress) == PAD_ERROR_DEVICE_NOT_CONNECTED);

#ifdef _WIN32
    Require(VirtualFree(watchedPage, 0, MEM_RELEASE) != 0);
#else
    if (GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) {
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(watchedPage, static_cast<std::size_t>(pageSize));
    }
    Require(munmap(watchedPage, static_cast<std::size_t>(pageSize)) == 0);
#endif

    Require(scePadClose_nid_postfix(handle) == PAD_OK);
}

}

int main() {
    TestPortIndexLimits();
    TestDescriptorBounds();
    TestTransactionalOutputPointerSafety();
    return 0;
}
