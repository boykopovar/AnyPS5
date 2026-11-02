#include "prx/libSceNpMatching2/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <unordered_set>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static constexpr int SCE_NP_MATCHING2_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80550C02);
static constexpr int SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED = static_cast<int>(0x80550C03);
static constexpr int SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80550C0A);
static constexpr int SCE_NP_MATCHING2_ERROR_CONTEXT_MAX = static_cast<int>(0x80550C04);
static constexpr int SCE_NP_MATCHING2_ERROR_CONTEXT_NOT_STARTED = static_cast<int>(0x80550C08);
static constexpr int SCE_NP_MATCHING2_ERROR_INVALID_CONTEXT_ID = static_cast<int>(0x80550C0B);
static constexpr int SCE_NP_MATCHING2_ERROR_CONTEXT_NOT_FOUND = static_cast<int>(0x80550C06);
static constexpr int SCE_NP_MATCHING2_ERROR_INVALID_SERVER_ID = static_cast<int>(0x80550C0C);
static constexpr int SCE_NP_MATCHING2_ERROR_INVALID_ROOM_ID = static_cast<int>(0x80550C0F);
static constexpr int SCE_NP_MATCHING2_ERROR_SERVER_NOT_AVAILABLE = static_cast<int>(0x80550C28);
static constexpr int SCE_NP_MATCHING2_ERROR_SESSION_NOT_FOUND = static_cast<int>(0x80550C3A);
static constexpr int SCE_NP_MATCHING2_SIGNALING_ERROR_CTX_NOT_FOUND = static_cast<int>(0x80550E05);
static constexpr int SCE_NP_MATCHING2_SIGNALING_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80550E15);
static constexpr int SCE_NP_MATCHING2_SIGNALING_ERROR_PEER_NOT_FOUND = static_cast<int>(0x80550E19);
static constexpr int SCE_NET_ERROR_RESOLVER_ETIMEDOUT = static_cast<int>(0x804101E2);
static constexpr std::int32_t SCE_NP_SIGNALING_CONN_STATUS_INACTIVE = 0;
static constexpr int MAX_CONTEXTS = 10;
static constexpr std::uint16_t CONTEXT_A_FLAG = 0x100;
static constexpr std::uint64_t CREATE_CONTEXT_PARAMETER_SIZE = 0x28;
static constexpr std::uint64_t CREATE_CONTEXT_PARAMETER_A_SIZE = 0x10;

namespace {

static_assert(offsetof(NpMatching2InitializeParameter, size) == 0x20);
static_assert(sizeof(NpMatching2InitializeParameter) == 0x30);

static_assert(sizeof(NpMatching2CreateContextParameter) == CREATE_CONTEXT_PARAMETER_SIZE);
static_assert(sizeof(NpMatching2CreateContextParameterA) == CREATE_CONTEXT_PARAMETER_A_SIZE);

std::mutex matching2Mutex;
bool matching2Initialized = false;
std::unordered_set<std::uint16_t> contexts;
int nextContextId = 1;
std::uint16_t signalingPort = 0;

enum class Leading { ServerId, RoomId };

int CheckContext(std::uint16_t ctxId) {
    if (!matching2Initialized) return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    return contexts.contains(ctxId) ? 0 : SCE_NP_MATCHING2_ERROR_INVALID_CONTEXT_ID;
}

int AllocateContext(std::uint16_t* ctxId, std::uint16_t flag) {
    for (int tried = 0; tried < MAX_CONTEXTS; ++tried) {
        const auto candidate = static_cast<std::uint16_t>((nextContextId - 1 + tried) % MAX_CONTEXTS + 1);
        if (contexts.contains(candidate) || contexts.contains(candidate | CONTEXT_A_FLAG)) continue;
        contexts.insert(candidate | flag);
        nextContextId = candidate % MAX_CONTEXTS + 1;
        *ctxId = candidate | flag;
        return 0;
    }
    return SCE_NP_MATCHING2_ERROR_CONTEXT_MAX;
}

int CheckRequest(const void* request, const std::uint32_t* requestId, Leading leading) {
    if (!matching2Initialized) return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (!request || !requestId) return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (leading == Leading::ServerId) {
        std::uint16_t serverId;
        std::memcpy(&serverId, request, sizeof(serverId));
        return serverId == 0 ? SCE_NP_MATCHING2_ERROR_INVALID_SERVER_ID : 0;
    }
    std::uint64_t roomId;
    std::memcpy(&roomId, request, sizeof(roomId));
    return roomId == 0 ? SCE_NP_MATCHING2_ERROR_INVALID_ROOM_ID : 0;
}

int StartRequest(const char* name, std::uint16_t ctxId, const void* request, const std::uint32_t* requestId, Leading leading) {
    std::lock_guard lock(matching2Mutex);
    if (const int error = CheckRequest(request, requestId, leading)) return error;
    if (!contexts.contains(ctxId)) return SCE_NP_MATCHING2_ERROR_CONTEXT_NOT_FOUND;
    NotImplemented_nid_no_patch(name);
    return 0;
}

int CheckLocal(std::uint16_t ctxId) {
    return contexts.contains(ctxId) ? SCE_NP_MATCHING2_ERROR_CONTEXT_NOT_STARTED : SCE_NP_MATCHING2_ERROR_CONTEXT_NOT_FOUND;
}

int ConnectionInfo(std::uint16_t ctxId, const void* info) {
    std::lock_guard lock(matching2Mutex);
    if (!info) return SCE_NP_MATCHING2_SIGNALING_ERROR_INVALID_ARGUMENT;
    return contexts.contains(ctxId) ? SCE_NP_MATCHING2_SIGNALING_ERROR_PEER_NOT_FOUND : SCE_NP_MATCHING2_SIGNALING_ERROR_CTX_NOT_FOUND;
}

}

extern "C" {

int APS5_VABI sceNpMatching2ContextStart(std::uint16_t ctxId, std::uint64_t timeout) {
    (void)ctxId;
    (void)timeout;
    std::lock_guard lock(matching2Mutex);
    if (!matching2Initialized) return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    return SCE_NET_ERROR_RESOLVER_ETIMEDOUT;
}

int APS5_VABI sceNpMatching2ContextStop(std::uint16_t ctxId) {
    std::lock_guard lock(matching2Mutex);
    if (const int error = CheckContext(ctxId)) return error;
    return SCE_NP_MATCHING2_ERROR_CONTEXT_NOT_STARTED;
}

int APS5_VABI sceNpMatching2CreateContext(const NpMatching2CreateContextParameter* param, std::uint16_t* ctxId) {
    std::lock_guard lock(matching2Mutex);
    if (!matching2Initialized) return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (!param || param->size != CREATE_CONTEXT_PARAMETER_SIZE || !ctxId) return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    return AllocateContext(ctxId, 0);
}

int APS5_VABI sceNpMatching2CreateJoinRoom() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2DestroyContext(std::uint16_t ctxId) {
    std::lock_guard lock(matching2Mutex);
    if (const int error = CheckContext(ctxId)) return error;
    contexts.erase(ctxId);
    return 0;
}

int APS5_VABI sceNpMatching2GetRoomDataExternalList() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2GetRoomDataInternal(std::uint16_t ctxId, const void* request, const void* requestOpt, std::uint32_t* requestId) {
    (void)requestOpt;
    return StartRequest(__func__, ctxId, request, requestId, Leading::RoomId);
}

int APS5_VABI sceNpMatching2GetRoomMemberDataInternal() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2GetServerId(std::uint16_t ctxId, std::uint16_t* serverId) {
    std::lock_guard lock(matching2Mutex);
    if (!matching2Initialized) return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (!serverId) return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    return CheckLocal(ctxId);
}

int APS5_VABI sceNpMatching2GetUserInfoList() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2GetWorldInfoList(std::uint16_t ctxId, const void* request, const void* requestOpt, std::uint32_t* requestId) {
    (void)requestOpt;
    return StartRequest(__func__, ctxId, request, requestId, Leading::ServerId);
}

int APS5_VABI sceNpMatching2Initialize(const NpMatching2InitializeParameter* param) {
    std::lock_guard lock(matching2Mutex);
    if (matching2Initialized) return SCE_NP_MATCHING2_ERROR_ALREADY_INITIALIZED;
    if (!param || (param->size != offsetof(NpMatching2InitializeParameter, sslPoolSize) && param->size != sizeof(NpMatching2InitializeParameter))) return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    matching2Initialized = true;
    return 0;
}

int APS5_VABI sceNpMatching2JoinRoom() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2KickoutRoomMember() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2LeaveRoom(std::uint16_t ctxId, const void* request, const void* requestOpt, std::uint32_t* requestId) {
    (void)requestOpt;
    std::lock_guard lock(matching2Mutex);
    if (const int error = CheckRequest(request, requestId, Leading::RoomId)) return error;
    return contexts.contains(ctxId) ? SCE_NP_MATCHING2_ERROR_SESSION_NOT_FOUND : SCE_NP_MATCHING2_ERROR_CONTEXT_NOT_FOUND;
}

int APS5_VABI sceNpMatching2RegisterContextCallback(void* callback, void* userdata) {
    (void)callback;
    (void)userdata;
    std::lock_guard lock(matching2Mutex);
    return matching2Initialized ? 0 : SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceNpMatching2RegisterRoomEventCallback(std::uint16_t ctxId, void* callback, void* userdata) {
    (void)callback;
    (void)userdata;
    std::lock_guard lock(matching2Mutex);
    return CheckContext(ctxId);
}

int APS5_VABI sceNpMatching2RegisterRoomMessageCallback(std::uint16_t ctxId, void* callback, void* userdata) {
    (void)callback;
    (void)userdata;
    std::lock_guard lock(matching2Mutex);
    return CheckContext(ctxId);
}

int APS5_VABI sceNpMatching2RegisterSignalingCallback(std::uint16_t ctxId, void* callback, void* userdata) {
    (void)callback;
    (void)userdata;
    std::lock_guard lock(matching2Mutex);
    return CheckContext(ctxId);
}

int APS5_VABI sceNpMatching2SearchRoom() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2SendRoomChatMessage() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2SendRoomMessage() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2SetDefaultRequestOptParam(std::uint16_t ctxId, const void* requestOpt) {
    std::lock_guard lock(matching2Mutex);
    if (!matching2Initialized) return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (!requestOpt) return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    return CheckContext(ctxId);
}

int APS5_VABI sceNpMatching2SetRoomDataExternal(std::uint16_t ctxId, const void* request, const void* requestOpt, std::uint32_t* requestId) {
    (void)requestOpt;
    return StartRequest(__func__, ctxId, request, requestId, Leading::RoomId);
}

int APS5_VABI sceNpMatching2SetRoomDataInternal(std::uint16_t ctxId, const void* request, const void* requestOpt, std::uint32_t* requestId) {
    (void)requestOpt;
    return StartRequest(__func__, ctxId, request, requestId, Leading::RoomId);
}

int APS5_VABI sceNpMatching2SetRoomMemberDataInternal() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2SetUserInfo(std::uint16_t ctxId, const void* request, const void* requestOpt, std::uint32_t* requestId) {
    (void)requestOpt;
    return StartRequest(__func__, ctxId, request, requestId, Leading::ServerId);
}

int APS5_VABI sceNpMatching2SignalingGetConnectionInfo(std::uint16_t ctxId, std::uint64_t roomId, std::uint16_t memberId, std::int32_t infoType, void* info) {
    (void)roomId;
    (void)memberId;
    (void)infoType;
    return ConnectionInfo(ctxId, info);
}

int APS5_VABI sceNpMatching2Terminate(void) {
    std::lock_guard lock(matching2Mutex);
    if (!matching2Initialized) return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    matching2Initialized = false;
    contexts.clear();
    nextContextId = 1;
    return 0;
}

int APS5_VABI sceNpMatching2AbortContextStart(std::uint16_t ctxId) {
    (void)ctxId;
    std::lock_guard lock(matching2Mutex);
    return matching2Initialized ? 0 : SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceNpMatching2CreateContextA(const NpMatching2CreateContextParameterA* param, std::uint16_t* ctxId) {
    std::lock_guard lock(matching2Mutex);
    if (!matching2Initialized) return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (!param || param->size != CREATE_CONTEXT_PARAMETER_A_SIZE || !ctxId) return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    return AllocateContext(ctxId, CONTEXT_A_FLAG);
}

int APS5_VABI sceNpMatching2CreateContextInternal(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2CreateJoinRoomA(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2GetLobbyInfoList(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2GetLobbyMemberDataInternal(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2GetLobbyMemberDataInternalList(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2GetMemoryInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2GetRoomJoinedSlotMaskLocal(std::uint16_t ctxId, std::uint64_t roomId, std::uint64_t* joinedSlotMask) {
    std::lock_guard lock(matching2Mutex);
    if (!matching2Initialized) return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (!joinedSlotMask) return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (!contexts.contains(ctxId)) return SCE_NP_MATCHING2_ERROR_CONTEXT_NOT_FOUND;
    return roomId == 0 ? SCE_NP_MATCHING2_ERROR_INVALID_SERVER_ID : SCE_NP_MATCHING2_ERROR_CONTEXT_NOT_STARTED;
}

int APS5_VABI sceNpMatching2GetRoomMemberDataExternalList(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2GetRoomMemberIdListLocal(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2GetRoomPasswordLocal(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2GetSignalingOptParamLocal(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2GetSslMemoryInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2GetUserInfoListA(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2GetWorldIdArrayForAllServers(std::uint16_t ctxId, void* worldIds, std::uint64_t worldIdsNum, void* resultNum) {
    (void)worldIdsNum;
    std::lock_guard lock(matching2Mutex);
    if (!matching2Initialized) return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (!contexts.contains(ctxId)) return SCE_NP_MATCHING2_ERROR_CONTEXT_NOT_FOUND;
    if (!worldIds || !resultNum) return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    return SCE_NP_MATCHING2_ERROR_SERVER_NOT_AVAILABLE;
}

int APS5_VABI sceNpMatching2GrantRoomOwner(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2JoinLobby(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2JoinRoomA(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2LeaveLobby(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2RegisterLobbyEventCallback(std::uint16_t ctxId, void* callback, void* userdata) {
    (void)callback;
    (void)userdata;
    std::lock_guard lock(matching2Mutex);
    return CheckContext(ctxId);
}

int APS5_VABI sceNpMatching2RegisterLobbyMessageCallback(std::uint16_t ctxId, void* callback, void* userdata) {
    (void)callback;
    (void)userdata;
    std::lock_guard lock(matching2Mutex);
    return CheckContext(ctxId);
}

int APS5_VABI sceNpMatching2RegisterManualUdpSignalingCallback(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2SendLobbyChatMessage(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2SetExtraInitParam(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2SetLobbyMemberDataInternal(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2SetRoomDataInternalExt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2SetSignalingOptParam(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2SignalingAbortConnection(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2SignalingCancelPeerNetInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2SignalingEnableManualUdpMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2SignalingEstablishConnection(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2SignalingGetConnectionInfoA(std::uint16_t ctxId, std::uint64_t roomId, std::uint16_t memberId, std::int32_t infoType, void* info) {
    (void)roomId;
    (void)memberId;
    (void)infoType;
    return ConnectionInfo(ctxId, info);
}

int APS5_VABI sceNpMatching2SignalingGetConnectionStatus(std::uint16_t ctxId, std::uint64_t roomId, std::uint16_t memberId, std::int32_t* connStatus, void* peerAddr, std::uint16_t* peerPort) {
    (void)roomId;
    (void)memberId;
    (void)peerAddr;
    (void)peerPort;
    std::lock_guard lock(matching2Mutex);
    if (!matching2Initialized) return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (!connStatus) NotImplemented_nid_no_patch(__func__);
    if (const int error = CheckContext(ctxId)) return error;
    *connStatus = SCE_NP_SIGNALING_CONN_STATUS_INACTIVE;
    return 0;
}

int APS5_VABI sceNpMatching2SignalingGetLocalNetInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2SignalingGetPeerNetInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2SignalingGetPeerNetInfoResult(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2SignalingGetPingInfo(std::uint16_t ctxId, const void* request, const void* requestOpt, std::uint32_t* requestId) {
    (void)requestOpt;
    return StartRequest(__func__, ctxId, request, requestId, Leading::RoomId);
}

int APS5_VABI sceNpMatching2SignalingGetPort(std::uint16_t* port) {
    std::lock_guard lock(matching2Mutex);
    *port = signalingPort;
    return 0;
}

int APS5_VABI sceNpMatching2SignalingSetPort(std::uint16_t port) {
    std::lock_guard lock(matching2Mutex);
    signalingPort = port;
    return 0;
}

}
