# libSceNpSns

## add Functional

- [sceNpSnsIntCreateRequest..sceNpSnsYouTubeGetAccessToken](../../../core/libs/prx/libSceNpSns/Export.cpp) (libSceNpSns) - Verified ABI signatures and firmware 13.60 pre-init, null-pointer, struct-size (`0x30` Twitch / `0xc30` YouTube), reserved-byte, and request-lifecycle checks (create, delete, ownership, and aborted-state rejection) from `libSceNpSns.sprx`, with `GuestCopyIn_nid_no_patch`/`GuestCopyOut_nid_no_patch` fault safety on guest pointers. Concurrent in-progress request-use rejection is omitted while PSN IPMI execution remains unimplemented (`NotImplemented_nid_no_patch`).
