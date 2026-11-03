#include "prx/libSceNpMatching2/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif





namespace {

constexpr int ResolverTimedOut = static_cast<int>(0x804101E2u);
constexpr int AlreadyInitialized = static_cast<int>(0x80550C02u);
constexpr int NotInitialized = static_cast<int>(0x80550C03u);
constexpr int ContextMax = static_cast<int>(0x80550C04u);
constexpr int ContextNotStarted = static_cast<int>(0x80550C08u);
constexpr int InvalidArgument = static_cast<int>(0x80550C0Au);
constexpr int InvalidContextId = static_cast<int>(0x80550C0Bu);
constexpr int ContextNotFound = static_cast<int>(0x80550C06u);
constexpr int InvalidServerId = static_cast<int>(0x80550C0Cu);
constexpr int InvalidRoomId = static_cast<int>(0x80550C0Fu);
constexpr int ServerNotAvailable = static_cast<int>(0x80550C28u);
constexpr int SessionNotFound = static_cast<int>(0x80550C3Au);
constexpr int SignalingContextNotFound = static_cast<int>(0x80550E05u);
constexpr int SignalingInvalidArgument = static_cast<int>(0x80550E15u);
constexpr int SignalingPeerNotFound = static_cast<int>(0x80550E19u);
constexpr std::int32_t ConnStatusInactive = 0;
constexpr std::int32_t StatusSentinel = 0x5A5A5A5A;
constexpr std::uint32_t PeerAddrSentinel = 0xA5A5A5A5u;
constexpr std::uint16_t PeerPortSentinel = 0xA5A5;

int failures = 0;

void Require(bool value, const char* name, const char* what) {
    if (value) return;
    std::fprintf(stderr, "%s: %s\n", name, what);
    ++failures;
}

using CreateContextAFn = decltype(&sceNpMatching2CreateContextA);
using RegisterLobbyCallbackFn = int (APS5_VABI*)(std::uint16_t, void*, void*);
using RequestFn = int (APS5_VABI*)(std::uint16_t, const void*, const void*, std::uint32_t*);
using GetServerIdFn = int (APS5_VABI*)(std::uint16_t, std::uint16_t*);
using SlotMaskFn = int (APS5_VABI*)(std::uint16_t, std::uint64_t, std::uint64_t*);
using WorldIdsFn = int (APS5_VABI*)(std::uint16_t, void*, std::uint64_t, void*);
using ConnectionInfoFn = int (APS5_VABI*)(std::uint16_t, std::uint64_t, std::uint16_t, std::int32_t, void*);
using GetPortFn = int (APS5_VABI*)(std::uint16_t*);
using SetPortFn = int (APS5_VABI*)(std::uint16_t);

template <typename Function>
Function Resolve(const char* name) {
#ifdef _WIN32
    void* symbol = reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("libSceNpMatching2.prx"), name));
#else
    void* symbol = dlsym(RTLD_DEFAULT, name);
#endif
    if (!symbol) {
        std::fprintf(stderr, "%s is not exported by libSceNpMatching2\n", name);
        std::abort();
    }
    return reinterpret_cast<Function>(symbol);
}

void CheckOfflineRequests(CreateContextAFn createContextA) {
    const auto getWorldInfoList = Resolve<RequestFn>("sceNpMatching2GetWorldInfoList");
    const auto setUserInfo = Resolve<RequestFn>("sceNpMatching2SetUserInfo");
    const auto setRoomDataExternal = Resolve<RequestFn>("sceNpMatching2SetRoomDataExternal");
    const auto setRoomDataInternal = Resolve<RequestFn>("sceNpMatching2SetRoomDataInternal");
    const auto getRoomDataInternal = Resolve<RequestFn>("sceNpMatching2GetRoomDataInternal");
    const auto getPingInfo = Resolve<RequestFn>("sceNpMatching2SignalingGetPingInfo");
    const auto leaveRoom = Resolve<RequestFn>("sceNpMatching2LeaveRoom");
    const auto getServerId = Resolve<GetServerIdFn>("sceNpMatching2GetServerId");
    const auto getSlotMask = Resolve<SlotMaskFn>("sceNpMatching2GetRoomJoinedSlotMaskLocal");
    const auto getWorldIds = Resolve<WorldIdsFn>("sceNpMatching2GetWorldIdArrayForAllServers");
    const auto getConnectionInfo = Resolve<ConnectionInfoFn>("sceNpMatching2SignalingGetConnectionInfo");
    const auto getConnectionInfoA = Resolve<ConnectionInfoFn>("sceNpMatching2SignalingGetConnectionInfoA");
    const auto getPort = Resolve<GetPortFn>("sceNpMatching2SignalingGetPort");
    const auto setPort = Resolve<SetPortFn>("sceNpMatching2SignalingSetPort");
    unsigned char serverRequest[32] = {1};
    unsigned char roomRequest[64] = {5};
    unsigned char zeroed[64] = {};
    std::uint32_t requestId = 77;

    Require(getWorldInfoList(1, serverRequest, nullptr, &requestId) == NotInitialized && requestId == 77, "sceNpMatching2GetWorldInfoList", "a request before initialization did not return NOT_INITIALIZED");
    NpMatching2InitializeParameter init{};
    init.size = 0x30;
    NpMatching2CreateContextParameter create{};
    create.size = 0x28;
    NpMatching2CreateContextParameterA createA{1, 0, 0x10};
    std::uint16_t context = 0;
    std::uint16_t contextA = 0;
    Require(sceNpMatching2Initialize(&init) == 0, "sceNpMatching2Initialize", "initializing for the requests failed");
    Require(sceNpMatching2CreateContext(&create, &context) == 0 && context == 1, "sceNpMatching2CreateContext", "the request context is not id 1");
    Require(createContextA(&createA, &contextA) == 0 && contextA == 0x102, "sceNpMatching2CreateContextA", "the request A context is not id 0x102");

    Require(getWorldInfoList(context, nullptr, nullptr, &requestId) == InvalidArgument, "sceNpMatching2GetWorldInfoList", "a null request was taken");
    Require(getWorldInfoList(context, serverRequest, nullptr, nullptr) == InvalidArgument, "sceNpMatching2GetWorldInfoList", "a null request id was taken");
    for (const RequestFn request : {getWorldInfoList, setUserInfo}) {
        Require(request(context, zeroed, nullptr, &requestId) == InvalidServerId, "server request", "server id 0 did not return INVALID_SERVER_ID");
        Require(request(99, zeroed, nullptr, &requestId) == InvalidServerId, "server request", "the server id was not checked before the context");
        Require(request(99, serverRequest, nullptr, &requestId) == ContextNotFound && request(0, serverRequest, nullptr, &requestId) == ContextNotFound, "server request", "an unknown context did not return CONTEXT_NOT_FOUND");
    }
    for (const RequestFn request : {setRoomDataExternal, setRoomDataInternal, getRoomDataInternal, getPingInfo, leaveRoom}) {
        Require(request(context, zeroed, nullptr, &requestId) == InvalidRoomId, "room request", "room id 0 did not return INVALID_ROOM_ID");
        Require(request(99, zeroed, nullptr, &requestId) == InvalidRoomId, "room request", "the room id was not checked before the context");
        Require(request(99, roomRequest, nullptr, &requestId) == ContextNotFound, "room request", "an unknown context did not return CONTEXT_NOT_FOUND");
    }
    Require(requestId == 77, "request", "a refused request wrote a request id");
    Require(leaveRoom(context, roomRequest, nullptr, &requestId) == SessionNotFound && leaveRoom(contextA, roomRequest, nullptr, &requestId) == SessionNotFound && requestId == 77, "sceNpMatching2LeaveRoom", "leaving with no room joined did not return SESSION_NOT_FOUND");

    std::uint16_t serverId = 0xA5A5;
    Require(getServerId(99, nullptr) == InvalidArgument, "sceNpMatching2GetServerId", "a null output was not refused before the context");
    Require(getServerId(99, &serverId) == ContextNotFound, "sceNpMatching2GetServerId", "an unknown context did not return CONTEXT_NOT_FOUND");
    Require(getServerId(context, &serverId) == ContextNotStarted && getServerId(contextA, &serverId) == ContextNotStarted && serverId == 0xA5A5, "sceNpMatching2GetServerId", "a never-started context did not return CONTEXT_NOT_STARTED without writing");
    std::uint64_t mask = 0xA5A5A5A5A5A5A5A5ull;
    Require(getSlotMask(99, 5, nullptr) == InvalidArgument, "sceNpMatching2GetRoomJoinedSlotMaskLocal", "a null mask was not refused before the context");
    Require(getSlotMask(context, 0, &mask) == InvalidServerId && getSlotMask(contextA, 0, &mask) == InvalidServerId, "sceNpMatching2GetRoomJoinedSlotMaskLocal", "room 0 did not return 0x80550C0C");
    Require(getSlotMask(99, 0, &mask) == ContextNotFound, "sceNpMatching2GetRoomJoinedSlotMaskLocal", "room 0 was checked before the context");
    Require(getSlotMask(99, 5, &mask) == ContextNotFound, "sceNpMatching2GetRoomJoinedSlotMaskLocal", "an unknown context did not return CONTEXT_NOT_FOUND");
    Require(getSlotMask(context, 5, &mask) == ContextNotStarted && mask == 0xA5A5A5A5A5A5A5A5ull, "sceNpMatching2GetRoomJoinedSlotMaskLocal", "a never-started context did not return CONTEXT_NOT_STARTED without writing");
    std::uint32_t worldIds[4] = {0xA5A5A5A5u, 0xA5A5A5A5u, 0xA5A5A5A5u, 0xA5A5A5A5u};
    std::uint32_t worldIdCount = 0xA5A5A5A5u;
    Require(getWorldIds(99, nullptr, 0, nullptr) == ContextNotFound, "sceNpMatching2GetWorldIdArrayForAllServers", "the context was not checked before the pointers");
    Require(getWorldIds(context, nullptr, 0, nullptr) == InvalidArgument, "sceNpMatching2GetWorldIdArrayForAllServers", "null pointers were taken");
    Require(getWorldIds(context, worldIds, 4, &worldIdCount) == ServerNotAvailable && worldIds[0] == 0xA5A5A5A5u && worldIdCount == 0xA5A5A5A5u, "sceNpMatching2GetWorldIdArrayForAllServers", "a never-started context did not return SERVER_NOT_AVAILABLE without writing");
    unsigned char info[64] = {};
    for (const ConnectionInfoFn connectionInfo : {getConnectionInfo, getConnectionInfoA}) {
        Require(connectionInfo(99, 5, 1, 1, nullptr) == SignalingInvalidArgument, "connection info", "a null info was not refused with 0x80550E15 before the context");
        Require(connectionInfo(99, 5, 1, 1, info) == SignalingContextNotFound, "connection info", "an unknown context did not return 0x80550E05");
        Require(connectionInfo(context, 5, 1, 1, info) == SignalingPeerNotFound, "connection info", "a never-started context did not return 0x80550E19");
    }
    std::uint16_t port = 0xA5A5;
    Require(getPort(&port) == 0 && port == 0, "sceNpMatching2SignalingGetPort", "the port did not start at 0");
    Require(setPort(3659) == 0 && getPort(&port) == 0 && port == 3659, "sceNpMatching2SignalingSetPort", "the port that was set is not reported");
    Require(sceNpMatching2Terminate() == 0, "sceNpMatching2Terminate", "terminating after the requests failed");
}

bool NullStatusThrows(std::uint16_t ctxId) {
    try {
        sceNpMatching2SignalingGetConnectionStatus(ctxId, 1, 1, nullptr, nullptr, nullptr);
    } catch (const std::runtime_error& error) {
        return std::string(error.what()).find("not implemented") != std::string::npos;
    }
    return false;
}

}

int main() {
    const auto createContextA = Resolve<CreateContextAFn>("sceNpMatching2CreateContextA");
    const auto registerLobbyEvent = Resolve<RegisterLobbyCallbackFn>("sceNpMatching2RegisterLobbyEventCallback");
    const auto registerLobbyMessage = Resolve<RegisterLobbyCallbackFn>("sceNpMatching2RegisterLobbyMessageCallback");
    NpMatching2InitializeParameter init{};
    NpMatching2CreateContextParameter create{};
    create.size = 0x28;
    NpMatching2CreateContextParameterA createA{1, 0, 0x10};
    unsigned char requestOpt[24] = {};
    std::uint16_t context = 77;

    Require(sceNpMatching2CreateContext(nullptr, nullptr) == NotInitialized, "sceNpMatching2CreateContext", "a call before initialization did not return NOT_INITIALIZED first");
    Require(sceNpMatching2DestroyContext(1) == NotInitialized, "sceNpMatching2DestroyContext", "a call before initialization did not return NOT_INITIALIZED");
    Require(sceNpMatching2ContextStart(1, 0) == NotInitialized, "sceNpMatching2ContextStart", "a call before initialization did not return NOT_INITIALIZED");
    Require(sceNpMatching2ContextStop(1) == NotInitialized, "sceNpMatching2ContextStop", "a call before initialization did not return NOT_INITIALIZED");
    Require(sceNpMatching2AbortContextStart(1) == NotInitialized, "sceNpMatching2AbortContextStart", "a call before initialization did not return NOT_INITIALIZED");
    std::int32_t status = StatusSentinel;
    Require(sceNpMatching2SignalingGetConnectionStatus(1, 1, 1, &status, nullptr, nullptr) == NotInitialized && status == StatusSentinel, "sceNpMatching2SignalingGetConnectionStatus", "a call before initialization did not return NOT_INITIALIZED without writing the status");
    Require(sceNpMatching2SignalingGetConnectionStatus(1, 1, 1, nullptr, nullptr, nullptr) == NotInitialized, "sceNpMatching2SignalingGetConnectionStatus", "a null status before initialization did not return NOT_INITIALIZED first");
    Require(sceNpMatching2RegisterContextCallback(nullptr, nullptr) == NotInitialized, "sceNpMatching2RegisterContextCallback", "a call before initialization did not return NOT_INITIALIZED");
    Require(sceNpMatching2RegisterRoomEventCallback(1, nullptr, nullptr) == NotInitialized, "sceNpMatching2RegisterRoomEventCallback", "a call before initialization did not return NOT_INITIALIZED");
    Require(sceNpMatching2RegisterRoomMessageCallback(1, nullptr, nullptr) == NotInitialized, "sceNpMatching2RegisterRoomMessageCallback", "a call before initialization did not return NOT_INITIALIZED");
    Require(sceNpMatching2RegisterSignalingCallback(1, nullptr, nullptr) == NotInitialized, "sceNpMatching2RegisterSignalingCallback", "a call before initialization did not return NOT_INITIALIZED");
    Require(createContextA(nullptr, nullptr) == NotInitialized, "sceNpMatching2CreateContextA", "a call before initialization did not return NOT_INITIALIZED first");
    Require(registerLobbyEvent(1, nullptr, nullptr) == NotInitialized, "sceNpMatching2RegisterLobbyEventCallback", "a call before initialization did not return NOT_INITIALIZED");
    Require(registerLobbyMessage(1, nullptr, nullptr) == NotInitialized, "sceNpMatching2RegisterLobbyMessageCallback", "a call before initialization did not return NOT_INITIALIZED");
    Require(sceNpMatching2SetDefaultRequestOptParam(1, nullptr) == NotInitialized, "sceNpMatching2SetDefaultRequestOptParam", "a call before initialization did not return NOT_INITIALIZED first");
    Require(sceNpMatching2Terminate() == NotInitialized, "sceNpMatching2Terminate", "terminating before initialization did not return NOT_INITIALIZED");
    Require(sceNpMatching2Initialize(nullptr) == InvalidArgument, "sceNpMatching2Initialize", "a null parameter did not return INVALID_ARGUMENT");
    init.size = 0x10;
    Require(sceNpMatching2Initialize(&init) == InvalidArgument, "sceNpMatching2Initialize", "an invalid parameter size did not return INVALID_ARGUMENT");
    init.size = 0x20;
    Require(sceNpMatching2Initialize(&init) == InvalidArgument, "sceNpMatching2Initialize", "an invalid size was taken");
    init.size = 0x30;
    Require(sceNpMatching2Initialize(&init) == 0, "sceNpMatching2Initialize", "initialization failed");
    Require(sceNpMatching2Initialize(&init) == AlreadyInitialized, "sceNpMatching2Initialize", "initializing when already initialized did not return ALREADY_INITIALIZED");

    Require(sceNpMatching2CreateContext(nullptr, &context) == InvalidArgument, "sceNpMatching2CreateContext", "a null parameter was taken");
    create.size = 0x30;
    Require(sceNpMatching2CreateContext(&create, &context) == InvalidArgument, "sceNpMatching2CreateContext", "parameter size 0x30 was taken");
    create.size = 0x28;
    Require(sceNpMatching2CreateContext(&create, nullptr) == InvalidArgument, "sceNpMatching2CreateContext", "a null output was taken");
    Require(context == 77, "sceNpMatching2CreateContext", "a refused call wrote the context id");
    Require(sceNpMatching2CreateContext(&create, &context) == 0 && context == 1, "sceNpMatching2CreateContext", "the first context is not id 1");
    Require(sceNpMatching2DestroyContext(1) == 0, "sceNpMatching2DestroyContext", "destroying context 1 failed");
    std::uint16_t created = 0;
    for (std::uint16_t expected = 2; expected <= 10; ++expected) {
        Require(sceNpMatching2CreateContext(&create, &created) == 0 && created == expected, "sceNpMatching2CreateContext", "a context id was reused before the ids up to 10 were handed out");
    }
    Require(sceNpMatching2CreateContext(&create, &created) == 0 && created == 1, "sceNpMatching2CreateContext", "after id 10 the allocation did not wrap to the free id 1");
    created = 77;
    Require(sceNpMatching2CreateContext(&create, &created) == ContextMax && created == 77, "sceNpMatching2CreateContext", "an eleventh context did not return CONTEXT_MAX");
    Require(sceNpMatching2DestroyContext(11) == InvalidContextId, "sceNpMatching2DestroyContext", "an unknown context was destroyed");

    Require(sceNpMatching2ContextStart(1, 0) == ResolverTimedOut, "sceNpMatching2ContextStart", "starting a context did not return the offline resolver timeout");
    Require(sceNpMatching2ContextStart(99, 0) == ResolverTimedOut, "sceNpMatching2ContextStart", "the offline answer did not come before the context check");
    Require(sceNpMatching2ContextStop(1) == ContextNotStarted, "sceNpMatching2ContextStop", "a context that never started did not return CONTEXT_NOT_STARTED");
    Require(sceNpMatching2ContextStop(99) == InvalidContextId, "sceNpMatching2ContextStop", "an unknown context was taken");

    Require(sceNpMatching2AbortContextStart(1) == 0, "sceNpMatching2AbortContextStart", "aborting the start of a context failed");
    Require(sceNpMatching2AbortContextStart(99) == 0, "sceNpMatching2AbortContextStart", "an unknown context id was refused");
    Require(sceNpMatching2ContextStop(1) == ContextNotStarted, "sceNpMatching2AbortContextStart", "the abort changed the start state of the context");

    std::uint32_t peerAddr = PeerAddrSentinel;
    std::uint16_t peerPort = PeerPortSentinel;
    status = StatusSentinel;
    Require(sceNpMatching2SignalingGetConnectionStatus(1, 0x1122334455667788ull, 3, &status, &peerAddr, &peerPort) == 0 && status == ConnStatusInactive, "sceNpMatching2SignalingGetConnectionStatus", "a member of a room that was never joined is not reported inactive");
    Require(peerAddr == PeerAddrSentinel && peerPort == PeerPortSentinel, "sceNpMatching2SignalingGetConnectionStatus", "the peer address or port was written for an inactive member");
    status = StatusSentinel;
    Require(sceNpMatching2SignalingGetConnectionStatus(1, 0, 0, &status, nullptr, nullptr) == 0 && status == ConnStatusInactive, "sceNpMatching2SignalingGetConnectionStatus", "null peer address and port outputs were refused");
    status = StatusSentinel;
    Require(sceNpMatching2SignalingGetConnectionStatus(99, 1, 1, &status, &peerAddr, &peerPort) == InvalidContextId && status == StatusSentinel, "sceNpMatching2SignalingGetConnectionStatus", "an unknown context was taken or its status written");
    Require(NullStatusThrows(1), "sceNpMatching2SignalingGetConnectionStatus", "a null status for a known context did not throw");
    Require(NullStatusThrows(99), "sceNpMatching2SignalingGetConnectionStatus", "a null status for an unknown context did not throw before the context check");
    Require(sceNpMatching2DestroyContext(2) == 0, "sceNpMatching2DestroyContext", "destroying context 2 failed");
    status = StatusSentinel;
    Require(sceNpMatching2SignalingGetConnectionStatus(2, 1, 1, &status, nullptr, nullptr) == InvalidContextId && status == StatusSentinel, "sceNpMatching2SignalingGetConnectionStatus", "a destroyed context was taken");

    Require(sceNpMatching2RegisterContextCallback(nullptr, nullptr) == 0, "sceNpMatching2RegisterContextCallback", "registering the context callback failed");
    Require(sceNpMatching2RegisterRoomEventCallback(99, nullptr, nullptr) == InvalidContextId, "sceNpMatching2RegisterRoomEventCallback", "an unknown context was taken");
    Require(sceNpMatching2RegisterRoomEventCallback(1, nullptr, nullptr) == 0, "sceNpMatching2RegisterRoomEventCallback", "registering failed");
    Require(sceNpMatching2RegisterRoomMessageCallback(99, nullptr, nullptr) == InvalidContextId, "sceNpMatching2RegisterRoomMessageCallback", "an unknown context was taken");
    Require(sceNpMatching2RegisterRoomMessageCallback(1, nullptr, nullptr) == 0, "sceNpMatching2RegisterRoomMessageCallback", "registering failed");
    Require(sceNpMatching2RegisterSignalingCallback(99, nullptr, nullptr) == InvalidContextId, "sceNpMatching2RegisterSignalingCallback", "an unknown context was taken");
    Require(sceNpMatching2RegisterSignalingCallback(1, nullptr, nullptr) == 0, "sceNpMatching2RegisterSignalingCallback", "registering failed");
    Require(registerLobbyEvent(99, nullptr, nullptr) == InvalidContextId, "sceNpMatching2RegisterLobbyEventCallback", "an unknown context was taken");
    Require(registerLobbyEvent(1, nullptr, nullptr) == 0, "sceNpMatching2RegisterLobbyEventCallback", "registering failed");
    Require(registerLobbyMessage(99, nullptr, nullptr) == InvalidContextId, "sceNpMatching2RegisterLobbyMessageCallback", "an unknown context was taken");
    Require(registerLobbyMessage(1, nullptr, nullptr) == 0, "sceNpMatching2RegisterLobbyMessageCallback", "registering failed");
    created = 77;
    Require(createContextA(nullptr, &created) == InvalidArgument, "sceNpMatching2CreateContextA", "a null parameter was taken");
    createA.size = 0x28;
    Require(createContextA(&createA, &created) == InvalidArgument, "sceNpMatching2CreateContextA", "parameter size 0x28 was taken");
    createA.size = 0x10;
    Require(createContextA(&createA, nullptr) == InvalidArgument, "sceNpMatching2CreateContextA", "a null output was taken");
    Require(created == 77, "sceNpMatching2CreateContextA", "a refused call wrote the context id");
    Require(createContextA(&createA, &created) == 0 && created == 0x102, "sceNpMatching2CreateContextA", "the freed slot 2 was not handed out as A context 0x102");
    created = 77;
    Require(createContextA(&createA, &created) == ContextMax && created == 77, "sceNpMatching2CreateContextA", "an eleventh context did not return CONTEXT_MAX");
    Require(sceNpMatching2ContextStop(0x102) == ContextNotStarted, "sceNpMatching2CreateContextA", "the context it created is not a regular context");
    Require(sceNpMatching2DestroyContext(0x102) == 0, "sceNpMatching2CreateContextA", "the A context was not destroyed");
    Require(sceNpMatching2CreateContext(&create, &created) == 0 && created == 2, "sceNpMatching2CreateContextA", "destroying the A context did not free slot 2");
    Require(sceNpMatching2SetDefaultRequestOptParam(99, nullptr) == InvalidArgument, "sceNpMatching2SetDefaultRequestOptParam", "a null option was not refused before the context");
    Require(sceNpMatching2SetDefaultRequestOptParam(99, requestOpt) == InvalidContextId, "sceNpMatching2SetDefaultRequestOptParam", "an unknown context was taken");
    Require(sceNpMatching2SetDefaultRequestOptParam(1, requestOpt) == 0, "sceNpMatching2SetDefaultRequestOptParam", "setting the default option failed");

    Require(sceNpMatching2Terminate() == 0, "sceNpMatching2Terminate", "termination failed");
    Require(sceNpMatching2Terminate() == NotInitialized, "sceNpMatching2Terminate", "terminating after termination did not return NOT_INITIALIZED");
    Require(sceNpMatching2DestroyContext(1) == NotInitialized, "sceNpMatching2DestroyContext", "a context was destroyed after termination");
    Require(sceNpMatching2AbortContextStart(1) == NotInitialized, "sceNpMatching2AbortContextStart", "a call after termination did not return NOT_INITIALIZED");
    status = StatusSentinel;
    Require(sceNpMatching2SignalingGetConnectionStatus(1, 1, 1, &status, nullptr, nullptr) == NotInitialized && status == StatusSentinel, "sceNpMatching2SignalingGetConnectionStatus", "a call after termination did not return NOT_INITIALIZED without writing the status");
    Require(sceNpMatching2Initialize(&init) == 0, "sceNpMatching2Initialize", "initializing again failed");
    Require(sceNpMatching2RegisterRoomEventCallback(5, nullptr, nullptr) == InvalidContextId, "sceNpMatching2Terminate", "a context created before termination is still found");
    Require(sceNpMatching2CreateContext(&create, &created) == 0 && created == 1, "sceNpMatching2Terminate", "termination did not restart the context ids at 1");
    Require(sceNpMatching2Terminate() == 0, "sceNpMatching2Terminate", "the second termination failed");
    Require(sceNpMatching2Terminate() == NotInitialized, "sceNpMatching2Terminate", "the third termination did not return NOT_INITIALIZED");
    Require(sceNpMatching2ContextStart(1, 0) == NotInitialized, "sceNpMatching2ContextStart", "a call after termination did not return NOT_INITIALIZED");
    Require(sceNpMatching2ContextStop(1) == NotInitialized, "sceNpMatching2ContextStop", "a call after termination did not return NOT_INITIALIZED");
    Require(sceNpMatching2RegisterContextCallback(nullptr, nullptr) == NotInitialized, "sceNpMatching2RegisterContextCallback", "a call after termination did not return NOT_INITIALIZED");
    Require(sceNpMatching2RegisterSignalingCallback(1, nullptr, nullptr) == NotInitialized, "sceNpMatching2RegisterSignalingCallback", "a call after termination did not return NOT_INITIALIZED");
    Require(registerLobbyEvent(1, nullptr, nullptr) == NotInitialized, "sceNpMatching2RegisterLobbyEventCallback", "a call after termination did not return NOT_INITIALIZED");
    Require(createContextA(&createA, &created) == NotInitialized, "sceNpMatching2CreateContextA", "a call after termination did not return NOT_INITIALIZED");

    CheckOfflineRequests(createContextA);

    if (failures != 0) return 1;
    std::puts("NpMatching2 offline tests passed");
    return 0;
}
