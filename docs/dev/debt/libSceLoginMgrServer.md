# libSceLoginMgrServer technical debt

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceLoginMgrServer/Export.cpp) - 5 entries present in both version-labelled images are import-resolution declarations only. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `GzbYqYyRTBg` |
| `Unknown` | `M1h7vFE7Hm0` |
| `Unknown` | `NvO5e-NOkXU` |
| `Unknown` | `T7HQTX-ylss` |
| `Unknown` | `uD-Sde+R4D8` |

### Unknown function info

- [sceLoginMgrServerCheckSafetyNoticeShown, sceLoginMgrServerClearMorpheusRequiredUserId, sceLoginMgrServerConvertToUserId, sceLoginMgrServerCreateUser, sceLoginMgrServerDestroyUser, sceLoginMgrServerDialogGetAppId, sceLoginMgrServerDialogGetOpenParam, sceLoginMgrServerDialogNotifyCloseFinished, sceLoginMgrServerDialogNotifyOpenFinished, sceLoginMgrServerDialogSetResult, sceLoginMgrServerGetLogoutInfo, sceLoginMgrServerGetMorpheusRequiredUserId, sceLoginMgrServerGetSharePlayAllowPadOperation, sceLoginMgrServerInitialize, sceLoginMgrServerInitializeSharePlayAllowPadOperation, sceLoginMgrServerIsRequestedCdlgClose, sceLoginMgrServerLoginServiceGetRequestParam, sceLoginMgrServerLoginServiceNotifyRequestFinished, sceLoginMgrServerNotifyEasySignInFinished, sceLoginMgrServerNotifyEasySignInUserCode, sceLoginMgrServerNotifyHmdConnectInfo, sceLoginMgrServerNotifySafetyNoticeShown, sceLoginMgrServerNotifyShellUIState, sceLoginMgrServerSetLoginFlag, sceLoginMgrServerSetMorpheusAssignEnabledFlag, sceLoginMgrServerSetMorpheusRequiredUserId, sceLoginMgrServerSetSharePlayMode, sceLoginMgrServerSetUserStatus, sceLoginMgrServerTerminate and sceLoginMgrServerUpdateUserIdRalatedToPadUniqueId](../../../core/libs/prx/libSceLoginMgrServer/Export.cpp) (libSceLoginMgrServer) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLoginMgrServer.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
- [sceLoginMgrServerNotifySafetyNoticeShownForGen2Vr, sceLoginMgrServerCheckSafetyNoticeShownForGen2Vr](../../../core/libs/prx/libSceLoginMgrServer/Export.cpp) have unknown signatures and behaviour. The parameterless `int` declarations are import-resolution placeholders, not confirmed ABI signatures, and every call throws through `NotImplemented_nid_no_patch`. Only the names and NIDs are established by the [13.60 export inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libsceloginmgrserver-9d8badaba5.jsonl). These are system/VR UI-facing inventory entries only; safety-notice handling, VR behaviour and title use remain unverified.
