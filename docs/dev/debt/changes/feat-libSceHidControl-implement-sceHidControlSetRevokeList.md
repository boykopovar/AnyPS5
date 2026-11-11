# libSceHidControl

## change sceHidControlSetRevokeList

- [sceHidControlSetRevokeList](../../../core/libs/prx/libSceHidControl/Export.cpp) - models the earlier 3.20 declaration forwarding to the revoke list control command (`0x8010484a`), matching `sceHidControlSetRevokeList2` parameter validation; console hardware behaviour for pre-13.42 SDK revisions remains unmeasured.
