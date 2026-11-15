# libSceNpManager

## add Unknown function info

- [NpOnlineId::SetNpOnlineIdStr text](../../../core/libs/prx/libSceNpManager/ValueClasses.cpp) (libSceNpManager) - the text is copied in with `CopyGuestString_nid_no_patch` capped at 17 bytes, which keeps the 16-character limit and its `SCE_NP_ERROR_ONLINE_ID_TOO_LONG`; unreadable text returns `SCE_NP_ERROR_INVALID_ARGUMENT` like null text and leaves the ID as it was, because the console's behaviour on unmapped memory is unmeasured.
