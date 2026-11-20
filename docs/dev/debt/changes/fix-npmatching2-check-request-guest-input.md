# libSceNpMatching2

## add Functional

- [sceNpMatching2GetWorldInfoList, sceNpMatching2SetUserInfo, sceNpMatching2SetRoomDataExternal, sceNpMatching2SetRoomDataInternal, sceNpMatching2GetRoomDataInternal, sceNpMatching2SignalingGetPingInfo, sceNpMatching2LeaveRoom](../../../core/libs/prx/libSceNpMatching2/Export.cpp) (libSceNpMatching2) - returning `0x80550C0A` (`SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT`) with host `errno` preserved when the request pointer cannot be read via `GuestCopyIn_nid_no_patch` is a host-safety fallback; console fault behaviour on unmapped request pointers is unmeasured.
