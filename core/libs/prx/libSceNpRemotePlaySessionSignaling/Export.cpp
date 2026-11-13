#include "prx/libSceNpRemotePlaySessionSignaling/include/Api.hpp"
#include "prx/libSceNpSessionSignaling/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorInvalidArgument = static_cast<int>(0x80553303u);
constexpr std::uint32_t RemotePlayPort = 0x217C;
constexpr std::uint32_t RemotePlayPortFlag = 1;

}

extern "C" {

int APS5_VABI sceNpRemotePlaySessionSignalingCreateJoinSession() {
    NotImplemented_nid_no_patch(__func__);
}

int32_t APS5_VABI sceNpRemotePlaySessionSignalingGetConnectionStatus(std::uint32_t contextId, std::uint32_t connectionId, std::int32_t* status, std::uint32_t* peerAddress, std::uint16_t* peerPort) {
    if (status == nullptr) {
        return ErrorInvalidArgument;
    }
    std::int32_t value = 0;
    const int32_t result = sceNpSessionSignalingGetConnectionStatus(contextId, connectionId, &value, peerAddress, peerPort);
    if (result < 0) {
        return result;
    }
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(status, &value, sizeof(value));
    errno = savedErrno;
    if (!copied) {
        return ErrorInvalidArgument;
    }
    return 0;
}

int APS5_VABI sceNpRemotePlaySessionSignalingGetCustomProperty(std::uint32_t contextId, std::uint32_t* property) {
    return sceNpSessionSignalingGetCustomProperty(contextId, property);
}

int32_t APS5_VABI sceNpRemotePlaySessionSignalingGetLocalNetInfo(std::uint32_t contextId, NpRemotePlaySessionSignalingLocalNetInfo* info) {
    if (info == nullptr) {
        return ErrorInvalidArgument;
    }
    SessionSignalingLocalNetInfo local{};
    const int32_t result = sceNpSessionSignalingGetLocalNetInfo(contextId, &local);
    if (result < 0) {
        return result;
    }
    NpRemotePlaySessionSignalingLocalNetInfo localOut{};
    localOut.localAddress = local.localAddress;
    localOut.mappedAddress = local.mappedAddress;
    localOut.mappedPort = local.mappedPort;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(info, &localOut, sizeof(localOut));
    errno = savedErrno;
    if (!copied) {
        return ErrorInvalidArgument;
    }
    return 0;
}

int APS5_VABI sceNpRemotePlaySessionSignalingGetMemoryInfo(SessionSignalingMemoryInfo* memoryInfo) {
    if (memoryInfo == nullptr) {
        return ErrorInvalidArgument;
    }
    SessionSignalingMemoryInfo local{};
    const int result = sceNpSessionSignalingGetMemoryInfo(&local);
    if (result < 0) {
        return result;
    }
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(memoryInfo, &local, sizeof(local));
    errno = savedErrno;
    if (!copied) {
        return ErrorInvalidArgument;
    }
    return 0;
}

int APS5_VABI sceNpRemotePlaySessionSignalingInitialize(const SessionSignalingInitParam* param) {
    if (param == nullptr) {
        return ErrorInvalidArgument;
    }
    SessionSignalingInitParam localParam{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&localParam, param, sizeof(localParam));
    errno = savedErrno;
    if (!copied) {
        return ErrorInvalidArgument;
    }
    const SessionSignalingInitParam copy{localParam.userId, localParam.poolSize, localParam.affinityMask, localParam.threadPriority,
        localParam.stackSize};
    return sceNpSessionSignalingInitializeWithPort(&copy, RemotePlayPort, RemotePlayPortFlag);
}

int APS5_VABI sceNpRemotePlaySessionSignalingJoinSession() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpRemotePlaySessionSignalingLeaveSession(std::uint32_t contextId) {
    return sceNpSessionSignalingDestroyContext(contextId);
}

int APS5_VABI sceNpRemotePlaySessionSignalingSetCustomProperty(std::uint32_t contextId, const std::uint32_t* property, std::uint32_t* previous) {
    return sceNpSessionSignalingSetCustomProperty(contextId, property, previous);
}

int APS5_VABI sceNpRemotePlaySessionSignalingTerminate(void) {
    return sceNpSessionSignalingTerminate();
}

int APS5_VABI sceNpRemotePlaySessionSignalingAbortConnection() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpRemotePlaySessionSignalingEstablishConnection() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpRemotePlaySessionSignalingManualUdpCreateJoinSession() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpRemotePlaySessionSignalingManualUdpJoinSession() {
    NotImplemented_nid_no_patch(__func__);
}

}
