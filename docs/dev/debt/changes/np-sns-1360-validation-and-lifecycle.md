# libSceNpSns

## add Functional

- [sceNpSnsIntCreateRequest..sceNpSnsYouTubeGetAccessToken](../../../core/libs/prx/libSceNpSns/Export.cpp) (libSceNpSns) - Verified ABI signatures and firmware 13.60 pre-init, null-pointer, struct-size (`0x30` Twitch / `0xc30` YouTube), reserved-byte, and request-lifecycle checks from `libSceNpSns.sprx`, with `GuestCopyIn_nid_no_patch`/`GuestCopyOut_nid_no_patch` fault safety on guest pointers. Unemulated PSN IPMI paths still call `NotImplemented_nid_no_patch`.
