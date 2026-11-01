#pragma once

#include "SceTypes.hpp"

struct NpMatching2InitializeParameter {
    std::uint64_t poolSize;
    std::uint64_t cpuAffinityMask;
    std::int32_t threadPriority;
    std::uint8_t padding[4];
    std::uint64_t threadStackSize;
    std::uint64_t size;
    std::uint64_t sslPoolSize;
};

struct NpMatching2CreateContextParameter {
    const void* npId;
    const void* npCommunicationId;
    const void* npPassphrase;
    std::uint32_t serviceLabel;
    std::uint64_t size;
};

extern "C" {

int APS5_VABI sceNpMatching2ContextStart(std::uint16_t ctxId, std::uint64_t timeout);
int APS5_VABI sceNpMatching2ContextStop(std::uint16_t ctxId);
int APS5_VABI sceNpMatching2CreateContext(const NpMatching2CreateContextParameter* param, std::uint16_t* ctxId);
int APS5_VABI sceNpMatching2CreateJoinRoom();
int APS5_VABI sceNpMatching2DestroyContext(std::uint16_t ctxId);
int APS5_VABI sceNpMatching2GetRoomDataExternalList();
int APS5_VABI sceNpMatching2GetRoomDataInternal();
int APS5_VABI sceNpMatching2GetRoomMemberDataInternal();
int APS5_VABI sceNpMatching2GetServerId();
int APS5_VABI sceNpMatching2GetUserInfoList();
int APS5_VABI sceNpMatching2GetWorldInfoList();
int APS5_VABI sceNpMatching2Initialize(const NpMatching2InitializeParameter* param);
int APS5_VABI sceNpMatching2JoinRoom();
int APS5_VABI sceNpMatching2KickoutRoomMember();
int APS5_VABI sceNpMatching2LeaveRoom();
int APS5_VABI sceNpMatching2RegisterContextCallback(void* callback, void* userdata);
int APS5_VABI sceNpMatching2RegisterRoomEventCallback(std::uint16_t ctxId, void* callback, void* userdata);
int APS5_VABI sceNpMatching2RegisterRoomMessageCallback(std::uint16_t ctxId, void* callback, void* userdata);
int APS5_VABI sceNpMatching2RegisterSignalingCallback(std::uint16_t ctxId, void* callback, void* userdata);
int APS5_VABI sceNpMatching2SearchRoom();
int APS5_VABI sceNpMatching2SendRoomChatMessage();
int APS5_VABI sceNpMatching2SendRoomMessage();
int APS5_VABI sceNpMatching2SetDefaultRequestOptParam(std::uint16_t ctxId, const void* requestOpt);
int APS5_VABI sceNpMatching2SetRoomDataExternal();
int APS5_VABI sceNpMatching2SetRoomDataInternal();
int APS5_VABI sceNpMatching2SetRoomMemberDataInternal();
int APS5_VABI sceNpMatching2SetUserInfo();
int APS5_VABI sceNpMatching2SignalingGetConnectionInfo();
int APS5_VABI sceNpMatching2Terminate(void);

int APS5_VABI sceNpMatching2AbortContextStart(std::uint16_t ctxId);
int APS5_VABI sceNpMatching2CreateContextA();
int APS5_VABI sceNpMatching2CreateContextInternal();
int APS5_VABI sceNpMatching2CreateJoinRoomA();
int APS5_VABI sceNpMatching2GetLobbyInfoList();
int APS5_VABI sceNpMatching2GetLobbyMemberDataInternal();
int APS5_VABI sceNpMatching2GetLobbyMemberDataInternalList();
int APS5_VABI sceNpMatching2GetMemoryInfo();
int APS5_VABI sceNpMatching2GetRoomJoinedSlotMaskLocal();
int APS5_VABI sceNpMatching2GetRoomMemberDataExternalList();
int APS5_VABI sceNpMatching2GetRoomMemberIdListLocal();
int APS5_VABI sceNpMatching2GetRoomPasswordLocal();
int APS5_VABI sceNpMatching2GetSignalingOptParamLocal();
int APS5_VABI sceNpMatching2GetSslMemoryInfo();
int APS5_VABI sceNpMatching2GetUserInfoListA();
int APS5_VABI sceNpMatching2GetWorldIdArrayForAllServers();
int APS5_VABI sceNpMatching2GrantRoomOwner();
int APS5_VABI sceNpMatching2JoinLobby();
int APS5_VABI sceNpMatching2JoinRoomA();
int APS5_VABI sceNpMatching2LeaveLobby();
int APS5_VABI sceNpMatching2RegisterLobbyEventCallback();
int APS5_VABI sceNpMatching2RegisterLobbyMessageCallback();
int APS5_VABI sceNpMatching2RegisterManualUdpSignalingCallback();
int APS5_VABI sceNpMatching2SendLobbyChatMessage();
int APS5_VABI sceNpMatching2SetExtraInitParam();
int APS5_VABI sceNpMatching2SetLobbyMemberDataInternal();
int APS5_VABI sceNpMatching2SetRoomDataInternalExt();
int APS5_VABI sceNpMatching2SetSignalingOptParam();
int APS5_VABI sceNpMatching2SignalingAbortConnection();
int APS5_VABI sceNpMatching2SignalingCancelPeerNetInfo();
int APS5_VABI sceNpMatching2SignalingEnableManualUdpMode();
int APS5_VABI sceNpMatching2SignalingEstablishConnection();
int APS5_VABI sceNpMatching2SignalingGetConnectionInfoA();
int APS5_VABI sceNpMatching2SignalingGetConnectionStatus(std::uint16_t ctxId, std::uint64_t roomId, std::uint16_t memberId, std::int32_t* connStatus, void* peerAddr, std::uint16_t* peerPort);
int APS5_VABI sceNpMatching2SignalingGetLocalNetInfo();
int APS5_VABI sceNpMatching2SignalingGetPeerNetInfo();
int APS5_VABI sceNpMatching2SignalingGetPeerNetInfoResult();
int APS5_VABI sceNpMatching2SignalingGetPingInfo();
int APS5_VABI sceNpMatching2SignalingGetPort();
int APS5_VABI sceNpMatching2SignalingSetPort();
}

