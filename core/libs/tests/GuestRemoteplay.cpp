#include "prx/libSceRemoteplay/libSceRemoteplay.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

static void Require(bool value) { if (!value) std::abort(); }

static void CheckPreInitGates(int notInitialized) {
    using GateFn = int (APS5_VABI*)();
    const GateFn gatedExports[] = {
        sceRemoteplayIsRemoteViewering,
        sceRemoteplayNotifyCanWakeup,
        sceRemoteplayRegistCallback,
        sceRemoteplayUnregistCallback,
        sceRemoteplayApprove,
        sceRemoteplayClearAllRegistData,
        sceRemoteplayClearConnectHistory,
        sceRemoteplayConfirmDeviceRegist,
        sceRemoteplayDisconnect,
        sceRemoteplayGeneratePinCode,
        sceRemoteplayGetApMode,
        sceRemoteplayGetConnectHistory,
        sceRemoteplayGetConnectUserId,
        sceRemoteplayGetMbusDeviceInfo,
        sceRemoteplayGetRemoteplayStatus,
        sceRemoteplayGetRpMode,
        sceRemoteplayImeClose,
        sceRemoteplayImeOpen,
        sceRemoteplayNotifyMbusDeviceRegistComplete,
        sceRemoteplayNotifyNpPushWakeup,
        sceRemoteplayNotifyPinCodeError,
        sceRemoteplayNotifySubmitCode,
        sceRemoteplayNotifyUserDelete,
        sceRemoteplayPrintAllRegistData,
        sceRemoteplayProhibit,
        sceRemoteplayServerLock,
        sceRemoteplayServerUnLock,
        sceRemoteplaySetApMode,
        sceRemoteplaySetLogLevel,
        sceRemoteplaySetProhibition,
        sceRemoteplaySetProhibitionForVsh,
        sceRemoteplaySetRpMode,
        sceRemoteplayDisableInputDevice,
        sceRemoteplayEnableInputDevice,
        sceRemoteplayGetClientInfo,
        libSceRemoteplayUnknown_HV7jZe1frbM_nid_no_patch,
    };
    for (const GateFn fn : gatedExports) {
        Require(fn() == notInitialized);
    }
    Require(sceRemoteplayIsRemotePlaying() == 0);
}

int main() {
    constexpr auto invalidArgument = static_cast<int>(0x80FC0001u);
    constexpr auto alreadyInitialized = static_cast<int>(0x80FC0003u);
    constexpr auto notInitialized = static_cast<int>(0x80FC0004u);
    constexpr int connectionStatusDisconnect = 0;
    std::array<unsigned char, 0x1800> heap{};

    int status = -1;
    Require(sceRemoteplayGetConnectionStatus(0, &status) == notInitialized && status == -1);
    Require(sceRemoteplayTerminate() == notInitialized);
    CheckPreInitGates(notInitialized);
    for (const int user : {-1, 2147483392}) {
        status = -1;
        Require(sceRemoteplayGetConnectionStatus(user, &status) == notInitialized && status == -1);
    }
    Require(sceRemoteplayGetConnectionStatus(0, nullptr) == notInitialized);
    Require(sceRemoteplayGetConnectionStatus(-1, nullptr) == notInitialized);

    Require(sceRemoteplayInitialize(nullptr, 0) == 0);
    Require(sceRemoteplayGetConnectionStatus(0, nullptr) == invalidArgument);
    Require(sceRemoteplayGetConnectionStatus(0, reinterpret_cast<int*>(0x10)) == invalidArgument);
    status = -1;
    Require(sceRemoteplayGetConnectionStatus(0, &status) == 0 && status == connectionStatusDisconnect);
    Require(sceRemoteplayInitialize(nullptr, 0) == alreadyInitialized);
    Require(sceRemoteplayInitialize(heap.data(), heap.size()) == alreadyInitialized);

    Require(sceRemoteplayTerminate() == 0);
    Require(sceRemoteplayTerminate() == notInitialized);
    CheckPreInitGates(notInitialized);
    status = -1;
    Require(sceRemoteplayGetConnectionStatus(0, &status) == notInitialized && status == -1);

    Require(sceRemoteplayInitialize(heap.data(), heap.size() - 1) == invalidArgument);
    Require(sceRemoteplayTerminate() == notInitialized);

    Require(sceRemoteplayInitialize(heap.data(), heap.size()) == 0);
    Require(sceRemoteplayTerminate() == 0);

    Require(sceRemoteplayInitialize(nullptr, 1) == 0);
    Require(sceRemoteplayTerminate() == 0);

    Require(sceRemoteplayInitialize(heap.data(), heap.size()) == 0);
    status = -1;
    Require(sceRemoteplayGetConnectionStatus(3, &status) == 0 && status == connectionStatusDisconnect);
    Require(sceRemoteplayTerminate() == 0);
    return 0;
}
