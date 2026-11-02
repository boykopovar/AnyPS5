#include "prx/libSceNpSessionSignaling/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <array>

namespace {

constexpr int InvalidArgument = static_cast<int>(0x80553303u);
constexpr int Unavailable = static_cast<int>(0x80552D06u);
constexpr int ConnectionNotFound = static_cast<int>(0x8055330Cu);
constexpr int NotInitialized = static_cast<int>(0x80553301u);
constexpr int AlreadyInitialized = static_cast<int>(0x80553302u);
constexpr int ContextLimit = static_cast<int>(0x80553307u);
constexpr int ContextNotFound = static_cast<int>(0x80553308u);
constexpr int GroupNotFound = static_cast<int>(0x8055330Au);

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "NpSessionSignaling: %s\n", message);
        std::abort();
    }
}

}

int main() {
    alignas(8) unsigned char initializeParam[40] = {};
    Require(sceNpSessionSignalingTerminate() == NotInitialized, "terminate before initialize");
    Require(sceNpSessionSignalingCreateContext2(nullptr, nullptr) == NotInitialized, "initialization check precedes context arguments");
    Require(sceNpSessionSignalingGetConnectionStatus(0, 0, nullptr, nullptr, nullptr) == NotInitialized, "status before initialize");
    Require(sceNpSessionSignalingGetMemoryInfo(nullptr) == NotInitialized, "memory info before initialize");
    Require(sceNpSessionSignalingGetConnectionStatistics(nullptr) == NotInitialized, "statistics before initialize");
    Require(sceNpSessionSignalingGetGroupFromPeerAddress(0, nullptr, nullptr) == NotInitialized, "group from peer before initialize");
    Require(sceNpSessionSignalingGetCustomProperty(0, nullptr) == NotInitialized, "custom property before initialize");
    Require(sceNpSessionSignalingSetCustomProperty(0, nullptr, nullptr) == NotInitialized, "set custom property before initialize");
    Require(sceNpSessionSignalingRegisterInternalCallback(0, nullptr, nullptr) == NotInitialized, "register callback before initialize");
    Require(sceNpSessionSignalingInitialize(nullptr) == InvalidArgument, "null initialize parameter");
    Require(sceNpSessionSignalingInitialize(reinterpret_cast<const SessionSignalingInitParam*>(initializeParam)) == InvalidArgument, "zero pool size");
    const std::uint64_t poolSize = 4 * 1024 * 1024;
    std::memcpy(initializeParam + 8, &poolSize, sizeof(poolSize));
    Require(sceNpSessionSignalingInitialize(reinterpret_cast<const SessionSignalingInitParam*>(initializeParam)) == 0, "initialize failed");
    SessionSignalingMemoryInfo memoryInfo{};
    Require(sceNpSessionSignalingGetMemoryInfo(nullptr) == InvalidArgument, "null memory info");
    Require(sceNpSessionSignalingGetMemoryInfo(&memoryInfo) == 0, "memory info failed");
    Require(memoryInfo.poolSize == poolSize && memoryInfo.usedSize == 0 && memoryInfo.peakUsedSize == 0, "memory info contents");
    SessionSignalingConnectionStatistics statistics{};
    statistics.peakConnectionCount = 1;
    statistics.connectionCount = 1;
    statistics.connectingCount = 1;
    statistics.connectedCount = 1;
    Require(sceNpSessionSignalingGetConnectionStatistics(nullptr) == InvalidArgument, "null statistics");
    Require(sceNpSessionSignalingGetConnectionStatistics(&statistics) == 0, "statistics failed");
    Require(statistics.peakConnectionCount == 0 && statistics.connectionCount == 0 && statistics.connectingCount == 0 && statistics.connectedCount == 0, "statistics contents");
    Require(sceNpSessionSignalingInitialize(nullptr) == AlreadyInitialized, "initialization check precedes initialize arguments");
    alignas(8) unsigned char contextParam[56] = {};
    std::uint32_t contextId = 0;
    Require(sceNpSessionSignalingCreateContext(nullptr, &contextId) == InvalidArgument, "null context parameter");
    Require(contextId == 0, "invalid context parameter wrote an id");
    Require(sceNpSessionSignalingCreateContext2(nullptr, &contextId) == InvalidArgument, "null context2 parameter");
    Require(contextId == 0, "invalid context2 parameter wrote an id");
    Require(sceNpSessionSignalingCreateContext(reinterpret_cast<const SessionSignalingContextParam1*>(contextParam), nullptr) == InvalidArgument, "null context id");
    Require(sceNpSessionSignalingCreateContext2(reinterpret_cast<const SessionSignalingContextParam2*>(contextParam), nullptr) == InvalidArgument, "null context2 id");
    Require(sceNpSessionSignalingCreateContext2(reinterpret_cast<const SessionSignalingContextParam2*>(contextParam), &contextId) == 0, "context creation failed");
    const auto firstId = contextId;
    Require(sceNpSessionSignalingCreateContext2(reinterpret_cast<const SessionSignalingContextParam2*>(contextParam), &contextId) == ContextLimit, "duplicate user context");
    Require(contextId == firstId, "duplicate context modified output");
    std::array<std::uint32_t, 9> otherContexts{};
    for (std::int32_t userId = 1; userId <= 9; ++userId) {
        std::memcpy(contextParam, &userId, sizeof(userId));
        Require(sceNpSessionSignalingCreateContext2(reinterpret_cast<const SessionSignalingContextParam2*>(contextParam), &otherContexts[userId - 1]) == 0, "create remaining contexts");
    }
    const std::int32_t overflowUser = 10;
    std::memcpy(contextParam, &overflowUser, sizeof(overflowUser));
    Require(sceNpSessionSignalingCreateContext2(reinterpret_cast<const SessionSignalingContextParam2*>(contextParam), &contextId) == ContextLimit, "context limit");
    Require(contextId == firstId, "context limit modified output");
    Require(sceNpSessionSignalingDestroyContext(firstId) == 0, "destroy first context");
    Require(sceNpSessionSignalingDestroyContext(firstId) == ContextNotFound, "destroy stale context");
    Require(sceNpSessionSignalingCreateContext2(reinterpret_cast<const SessionSignalingContextParam2*>(contextParam), &contextId) == 0, "reuse context slot");
    Require(contextId != firstId, "reused context kept stale id");
    std::uint32_t staleProperty = 0x12345678u;
    std::uint32_t previousProperty = 0;
    int callbackMarker = 0;
    int userDataMarker = 0;
    Require(sceNpSessionSignalingSetCustomProperty(contextId, &staleProperty, &previousProperty) == 0 && previousProperty == 0, "set property before context reuse");
    Require(sceNpSessionSignalingRegisterInternalCallback(contextId, &callbackMarker, &userDataMarker) == 0, "register callback before context reuse");
    Require(sceNpSessionSignalingDestroyContext(contextId) == 0, "destroy context before reuse two");
    const std::int32_t secondReuseUser = 11;
    std::memcpy(contextParam, &secondReuseUser, sizeof(secondReuseUser));
    Require(sceNpSessionSignalingCreateContext2(reinterpret_cast<const SessionSignalingContextParam2*>(contextParam), &contextId) == 0, "reuse context slot two");
    Require(sceNpSessionSignalingGetCustomProperty(contextId, &previousProperty) == 0 && previousProperty == 0, "reused context two kept state");
    Require(sceNpSessionSignalingSetCustomProperty(otherContexts[0], &staleProperty, &previousProperty) == 0 && previousProperty == 0, "set property before context one reuse");
    Require(sceNpSessionSignalingRegisterInternalCallback(otherContexts[0], &callbackMarker, &userDataMarker) == 0, "register callback before context one reuse");
    Require(sceNpSessionSignalingDestroyContext(otherContexts[0]) == 0, "destroy context before reuse one");
    const std::int32_t thirdReuseUser = 12;
    std::memcpy(contextParam, &thirdReuseUser, sizeof(thirdReuseUser));
    std::uint32_t reusedContext = 0;
    Require(sceNpSessionSignalingCreateContext(reinterpret_cast<const SessionSignalingContextParam1*>(contextParam), &reusedContext) == 0, "reuse context slot one");
    Require(sceNpSessionSignalingGetCustomProperty(reusedContext, &previousProperty) == 0 && previousProperty == 0, "reused context one kept state");

    std::uint32_t requestId = 0xa5a5a5a5u;
    Require(sceNpSessionSignalingRequestPrepare(firstId, &requestId) == ContextNotFound, "prepare stale context");
    Require(sceNpSessionSignalingRequestPrepare(firstId, nullptr) == InvalidArgument, "prepare argument precedence");
    Require(sceNpSessionSignalingRequestPrepare(contextId, nullptr) == InvalidArgument, "null request id");
    Require(sceNpSessionSignalingRequestPrepare(contextId, &requestId) == Unavailable, "prepare without network");
    Require(requestId == 0xa5a5a5a5u, "failed prepare wrote a request id");

    const auto netContextId = static_cast<std::int32_t>(contextId);
    unsigned char info[16];
    std::memset(info, 0xa5, sizeof(info));
    unsigned char untouched[sizeof(info)];
    std::memcpy(untouched, info, sizeof(info));
    Require(sceNpSessionSignalingGetLocalNetInfo(netContextId, nullptr) == InvalidArgument, "null info");
    Require(sceNpSessionSignalingGetLocalNetInfo(netContextId, reinterpret_cast<SessionSignalingLocalNetInfo*>(info)) == Unavailable, "local net info without network");
    Require(std::memcmp(info, untouched, sizeof(info)) == 0, "failed query modified the info");
    const unsigned char userPeer[40] = {};
    const auto* userPeerAddress = reinterpret_cast<const SessionSignalingPeerAddress*>(userPeer);
    std::uint32_t userConnection = 0xa5a5a5a5u;
    Require(sceNpSessionSignalingActivateUser(contextId, nullptr, &userConnection) == InvalidArgument, "activate user with a null peer address");
    Require(sceNpSessionSignalingActivateUser(contextId, userPeerAddress, nullptr) == InvalidArgument, "activate user with a null connection id");
    Require(sceNpSessionSignalingActivateUser(firstId, userPeerAddress, &userConnection) == ContextNotFound, "activate user on a stale context");
    Require(sceNpSessionSignalingActivateUser(contextId, userPeerAddress, &userConnection) == Unavailable, "activate user without network");
    Require(userConnection == 0xa5a5a5a5u, "a failed user activation wrote a connection id");
    Require(sceNpSessionSignalingGetConnectionInfo(contextId, 1, 1, nullptr) == InvalidArgument, "null connection info");
    Require(sceNpSessionSignalingGetConnectionInfo(firstId, 1, 1, reinterpret_cast<SessionSignalingConnectionInfo*>(info)) == ContextNotFound, "connection info on a stale context");
    Require(sceNpSessionSignalingGetConnectionInfo(contextId, 1, 1, reinterpret_cast<SessionSignalingConnectionInfo*>(info)) == ConnectionNotFound, "connection info without a session");
    Require(std::memcmp(info, untouched, sizeof(info)) == 0, "a failed connection info query modified the info");

    std::int32_t status = 0x5a5a5a5a;
    std::uint32_t peerAddress = 0xa5a5a5a5u;
    std::uint16_t peerPort = 0xa5a5;
    Require(sceNpSessionSignalingGetConnectionStatus(firstId, 1, &status, &peerAddress, &peerPort) == ContextNotFound, "status stale context");
    Require(sceNpSessionSignalingGetConnectionStatus(netContextId, 1, nullptr, &peerAddress, &peerPort) == InvalidArgument, "null connection status");
    Require(sceNpSessionSignalingGetConnectionStatus(netContextId, 1, &status, &peerAddress, &peerPort) == ConnectionNotFound, "connection without network");
    Require(sceNpSessionSignalingGetConnectionStatus(netContextId, 1, &status, nullptr, nullptr) == ConnectionNotFound, "connection without peer outputs");
    Require(status == 0x5a5a5a5a && peerAddress == 0xa5a5a5a5u && peerPort == 0xa5a5, "failed status query wrote an output");
    Require(sceNpSessionSignalingDeactivate(firstId, 1) == ContextNotFound, "deactivate stale context");
    Require(sceNpSessionSignalingDeactivate(contextId, 1) == GroupNotFound, "deactivate nonexistent group");

    Require(sceNpSessionSignalingGetGroupInfo(netContextId, 1, nullptr) == InvalidArgument, "null group info");
    Require(sceNpSessionSignalingGetGroupInfo(netContextId, 1, reinterpret_cast<SessionSignalingGroupInfo*>(info)) == GroupNotFound, "group info without a session");
    Require(std::memcmp(info, untouched, sizeof(info)) == 0, "failed group query modified the info");

    const unsigned char sessionId[48] = {};
    const unsigned char peer[40] = {};
    std::int32_t found = 0x5a5a5a5a;
    Require(sceNpSessionSignalingGetGroupFromSessionId(netContextId, nullptr, reinterpret_cast<std::uint32_t*>(&found)) == InvalidArgument, "null session id");
    Require(sceNpSessionSignalingGetGroupFromSessionId(netContextId, reinterpret_cast<const char*>(sessionId), nullptr) == InvalidArgument, "null group id");
    Require(sceNpSessionSignalingGetGroupFromSessionId(netContextId, reinterpret_cast<const char*>(sessionId), reinterpret_cast<std::uint32_t*>(&found)) == GroupNotFound, "group from session id without a session");
    Require(sceNpSessionSignalingGetConnectionFromNetAddress(netContextId, 0x0100007fu, 3658, nullptr) == InvalidArgument, "null connection id from net address");
    Require(sceNpSessionSignalingGetConnectionFromNetAddress(netContextId, 0x0100007fu, 3658, reinterpret_cast<std::uint32_t*>(&found)) == ConnectionNotFound, "connection from net address without a session");
    Require(sceNpSessionSignalingGetConnectionFromNetAddress2(netContextId, 1, 0x0100007fu, 3658, nullptr) == InvalidArgument, "null connection id from group net address");
    Require(sceNpSessionSignalingGetConnectionFromNetAddress2(netContextId, 1, 0x0100007fu, 3658, reinterpret_cast<std::uint32_t*>(&found)) == GroupNotFound, "connection from group net address without a session");
    Require(sceNpSessionSignalingGetConnectionFromPeerAddress(netContextId, nullptr, reinterpret_cast<std::uint32_t*>(&found)) == InvalidArgument, "null peer address");
    Require(sceNpSessionSignalingGetConnectionFromPeerAddress(netContextId, reinterpret_cast<const SessionSignalingPeerAddress*>(peer), nullptr) == InvalidArgument, "null connection id from peer address");
    Require(sceNpSessionSignalingGetConnectionFromPeerAddress(netContextId, reinterpret_cast<const SessionSignalingPeerAddress*>(peer), reinterpret_cast<std::uint32_t*>(&found)) == ConnectionNotFound, "connection from peer address without a session");
    Require(sceNpSessionSignalingGetConnectionFromPeerAddress2(netContextId, 1, nullptr, reinterpret_cast<std::uint32_t*>(&found)) == InvalidArgument, "null group peer address");
    Require(sceNpSessionSignalingGetConnectionFromPeerAddress2(netContextId, 1, reinterpret_cast<const SessionSignalingPeerAddress*>(peer), nullptr) == InvalidArgument, "null connection id from group peer address");
    Require(sceNpSessionSignalingGetConnectionFromPeerAddress2(netContextId, 1, reinterpret_cast<const SessionSignalingPeerAddress*>(peer), reinterpret_cast<std::uint32_t*>(&found)) == GroupNotFound, "connection from group peer address without a session");
    Require(sceNpSessionSignalingGetGroupFromPeerAddress(netContextId, nullptr, reinterpret_cast<std::uint32_t*>(&found)) == InvalidArgument, "null group peer address");
    Require(sceNpSessionSignalingGetGroupFromPeerAddress(netContextId, reinterpret_cast<const SessionSignalingPeerAddress*>(peer), nullptr) == InvalidArgument, "null group id from peer address");
    Require(sceNpSessionSignalingGetGroupFromPeerAddress(firstId, reinterpret_cast<const SessionSignalingPeerAddress*>(peer), reinterpret_cast<std::uint32_t*>(&found)) == ContextNotFound, "group from peer address stale context");
    Require(sceNpSessionSignalingGetGroupFromPeerAddress(netContextId, reinterpret_cast<const SessionSignalingPeerAddress*>(peer), reinterpret_cast<std::uint32_t*>(&found)) == GroupNotFound, "group from peer address without a session");
    Require(sceNpSessionSignalingGetGroupInfo(firstId, 1, reinterpret_cast<SessionSignalingGroupInfo*>(info)) == ContextNotFound, "group info stale context");
    Require(sceNpSessionSignalingGetGroupFromSessionId(firstId, reinterpret_cast<const char*>(sessionId), reinterpret_cast<std::uint32_t*>(&found)) == ContextNotFound, "group from session id stale context");
    Require(sceNpSessionSignalingGetConnectionFromNetAddress(firstId, 0x0100007fu, 3658, reinterpret_cast<std::uint32_t*>(&found)) == ContextNotFound, "connection from net address stale context");
    Require(sceNpSessionSignalingGetConnectionFromNetAddress2(firstId, 1, 0x0100007fu, 3658, reinterpret_cast<std::uint32_t*>(&found)) == ContextNotFound, "connection from group net address stale context");
    Require(sceNpSessionSignalingGetConnectionFromPeerAddress(firstId, reinterpret_cast<const SessionSignalingPeerAddress*>(peer), reinterpret_cast<std::uint32_t*>(&found)) == ContextNotFound, "connection from peer address stale context");
    Require(found == 0x5a5a5a5a, "a failed lookup wrote an id");
    std::uint32_t property = 0;
    Require(sceNpSessionSignalingGetCustomProperty(firstId, &property) == ContextNotFound, "custom property stale context");
    Require(sceNpSessionSignalingGetCustomProperty(contextId, nullptr) == 0, "null custom property output");
    Require(sceNpSessionSignalingGetCustomProperty(contextId, &property) == 0 && property == 0, "default custom property");
    Require(sceNpSessionSignalingSetCustomProperty(contextId, nullptr, nullptr) == InvalidArgument, "null custom property value");
    Require(sceNpSessionSignalingSetCustomProperty(firstId, &property, nullptr) == ContextNotFound, "set custom property stale context");
    std::uint32_t value = 0x12345678u;
    std::uint32_t previous = 0xa5a5a5a5u;
    Require(sceNpSessionSignalingSetCustomProperty(contextId, &value, &previous) == 0 && previous == 0, "set custom property");
    Require(sceNpSessionSignalingGetCustomProperty(contextId, &property) == 0 && property == value, "get custom property");
    value = 0xabcdef01u;
    Require(sceNpSessionSignalingSetCustomProperty(contextId, &value, &previous) == 0 && previous == 0x12345678u, "replace custom property");
    Require(sceNpSessionSignalingGetCustomProperty(contextId, &property) == 0 && property == value, "get replaced custom property");
    Require(sceNpSessionSignalingRegisterInternalCallback(firstId, nullptr, nullptr) == ContextNotFound, "register callback stale context");
    Require(sceNpSessionSignalingRegisterInternalCallback(contextId, nullptr, nullptr) == 0, "register callback");
    const char* const activateSessionId = reinterpret_cast<const char*>(sessionId);
    SessionSignalingSessionParam sessionParam{1, 0};
    std::uint32_t activatedGroup = 0xa5a5a5a5u;
    Require(sceNpSessionSignalingActivateSession(contextId, nullptr, 0, &sessionParam, &activatedGroup) == InvalidArgument, "activate with a null session id");
    Require(sceNpSessionSignalingActivateSession(contextId, activateSessionId, 0, nullptr, &activatedGroup) == InvalidArgument, "activate with a null session param");
    Require(sceNpSessionSignalingActivateSession(contextId, activateSessionId, 0, &sessionParam, nullptr) == InvalidArgument, "activate with a null group id");
    Require(sceNpSessionSignalingActivateSession(firstId, activateSessionId, 0, &sessionParam, &activatedGroup) == ContextNotFound, "activate on a stale context");
    const SessionSignalingSessionParam modeOneWithOption{1, 1};
    Require(sceNpSessionSignalingActivateSession(contextId, activateSessionId, 0, &modeOneWithOption, &activatedGroup) == InvalidArgument, "mode 1 with an option");
    const SessionSignalingSessionParam modeTwoWithoutOption{2, 0};
    Require(sceNpSessionSignalingActivateSession(contextId, activateSessionId, 0, &modeTwoWithoutOption, &activatedGroup) == InvalidArgument, "mode 2 without an option");
    Require(sceNpSessionSignalingActivateSession(contextId, activateSessionId, 0, &sessionParam, &activatedGroup) == Unavailable, "activate without network");
    Require(activatedGroup == 0xa5a5a5a5u, "a failed activation wrote a group id");

    Require(sceNpSessionSignalingTerminate() == 0, "terminate failed");
    Require(sceNpSessionSignalingDestroyContext(contextId) == NotInitialized, "destroy after terminate");
    Require(sceNpSessionSignalingActivateUser(contextId, nullptr, nullptr) == NotInitialized, "activate user after terminate");
    Require(sceNpSessionSignalingDeactivate(contextId, 1) == NotInitialized, "deactivate after terminate");
    Require(sceNpSessionSignalingGetConnectionInfo(contextId, 1, 1, nullptr) == NotInitialized, "connection info after terminate");
    Require(sceNpSessionSignalingGetConnectionStatus(contextId, 1, nullptr, nullptr, nullptr) == NotInitialized, "connection status after terminate");
    Require(sceNpSessionSignalingGetLocalNetInfo(contextId, nullptr) == NotInitialized, "local net info after terminate");
    Require(sceNpSessionSignalingGetGroupInfo(netContextId, 1, nullptr) == NotInitialized, "group info after terminate");
    Require(sceNpSessionSignalingGetGroupFromSessionId(netContextId, nullptr, nullptr) == NotInitialized, "group from session id after terminate");
    Require(sceNpSessionSignalingGetConnectionFromNetAddress(netContextId, 0, 0, nullptr) == NotInitialized, "connection from net address after terminate");
    Require(sceNpSessionSignalingGetConnectionFromNetAddress2(netContextId, 1, 0, 0, nullptr) == NotInitialized, "connection from group net address after terminate");
    Require(sceNpSessionSignalingGetConnectionFromPeerAddress(netContextId, nullptr, nullptr) == NotInitialized, "connection from peer address after terminate");
    Require(sceNpSessionSignalingGetConnectionFromPeerAddress2(netContextId, 1, nullptr, nullptr) == NotInitialized, "connection from peer address 2 after terminate");
    Require(sceNpSessionSignalingActivateSession(contextId, nullptr, 0, nullptr, nullptr) == NotInitialized, "activate after terminate");
    Require(sceNpSessionSignalingTerminate() == NotInitialized, "repeated terminate");
    Require(sceNpSessionSignalingInitialize(reinterpret_cast<const SessionSignalingInitParam*>(initializeParam)) == 0, "reinitialize");
    Require(sceNpSessionSignalingRequestPrepare(contextId, &requestId) == ContextNotFound, "reinitialize retained a context");
    Require(sceNpSessionSignalingTerminate() == 0, "final terminate");
    Require(sceNpSessionSignalingCreateContext(reinterpret_cast<const SessionSignalingContextParam1*>(1), &contextId) == NotInitialized, "create context post-terminate");
    Require(sceNpSessionSignalingInitializeWithPort(nullptr, 0x217b, 0) == InvalidArgument, "null initialize with port parameter");
    Require(sceNpSessionSignalingInitializeWithPort(reinterpret_cast<const SessionSignalingInitParam*>(initializeParam), 0x217b, 0) == 0, "initialize with port failed");
    Require(sceNpSessionSignalingInitializeWithPort(reinterpret_cast<const SessionSignalingInitParam*>(initializeParam), 0x217b, 0) == AlreadyInitialized, "initialize with port after initialize");
    Require(sceNpSessionSignalingGetMemoryInfo(&memoryInfo) == 0 && memoryInfo.poolSize == poolSize && memoryInfo.usedSize == 0 && memoryInfo.peakUsedSize == 0, "memory info after initialize with port");
    Require(sceNpSessionSignalingTerminate() == 0, "final terminate after initialize with port");
}
