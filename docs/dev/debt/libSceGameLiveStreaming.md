# libSceGameLiveStreaming technical debt

### Functional

- [sceGameLiveStreamingEnableLiveStreaming](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) - a call outside `sceGameLiveStreamingInitialize` and `sceGameLiveStreamingTerminate` throws, as the module's error for it is not confirmed (`sceGameLiveStreamingTerminate` and `sceGameLiveStreamingGetCurrentStatus2` return `0x80A00004`; [fpPS4](https://github.com/red-prig/fpPS4/blob/04cefd43e6fddd1ab033e7980cd356d14c964905/src/ps4_libscegamelivestreaming.pas#L18) does not return its `SCE_GAME_LIVE_STREAMING_ERROR_NOT_INITIALIZED` from this call). Otherwise both values are accepted and change nothing, as no broadcast is ever on air. The signature, `int sceGameLiveStreamingEnableLiveStreaming(const bool)`, follows [Orbital](https://github.com/AlexAltea/orbital/blob/cc408367da6a6ebbf5ac1ac559fefeadc73b2831/tools/ida/db_types.json#L810) and [fpPS4](https://github.com/red-prig/fpPS4/blob/04cefd43e6fddd1ab033e7980cd356d14c964905/src/ps4_libscegamelivestreaming.pas#L111)
- [libSceGameLiveStreaming](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) - native initialization requires exactly 0x4000 heap bytes before checking repeated initialization. Host lifecycle and pointer errors now match the wrapper, but service registration, allocation failures and asynchronous service readiness are not modelled. The host becomes ready immediately after initialization.
- [sceGameLiveStreamingGetCurrentStatus2](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) - the supplied module confirms the 72-byte layout and inactive status (user -1, remaining bytes zero). Live service counters, active-user changes and broadcast-resolution conversion are unimplemented.
- [sceGameLiveStreamingGetProgramInfo](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - the PS5 12.00 module clears the 0x134C-byte program info, asks the system service for its first 0xFD4 bytes and returns `0x80A00002` for a null pointer and `0x80A00004` before initialization. With nothing broadcast here the info stays zero, which assumes the service reports an empty program when nothing is on air; the field layout is unknown

### Unknown function info

- [sceGameLiveStreamingScreenInitialize](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingScreenInitializeSeparateModeParameter](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingScreenTerminate](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingScreenCloseSeparateMode](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingScreenConfigureSeparateMode](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingScreenOpenSeparateMode](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingScreenSetMode](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingPermitLiveStreaming](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingSetMaxBitrate](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingGetCurrentStatus](../../../core/libs/prx/libSceGameLiveStreaming/include/Api.hpp) - still throws; the meaning, division and SDK types of the first 12 bytes remain unknown. In the supplied `libSceGameLiveStreaming.sprx`, export `CoPMx369EqM` at 0x1810 takes one output pointer, clears 72 bytes, obtains service command 9's status, copies its first 12 bytes and clears bytes 0x0C..0x47. `GameLiveStreamingStatus` exposes this byte storage without assuming the `GetCurrentStatus2` field layout or SDK alignment. The decompiler's extra SIMD parameter is not an input: the assembly initializes the register before use.
- [sceGameLiveStreamingEnableSocialFeedback](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingGetSocialFeedbackMessagesCount](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingGetSocialFeedbackMessages](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingPermitServerSideRecording](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingSetMetadata](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingSetSpoilerTag](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingClearSpoilerTag](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingSetPresetSocialFeedbackCommands](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingClearPresetSocialFeedbackCommands](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingSetPresetSocialFeedbackCommandsDescription](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingClearSocialFeedbackMessages](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingPostSocialMessage](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingSetCameraFrameSetting](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingSetServiceProviderPermission](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingSetDefaultServiceProviderPermission](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingSetInvitationSessionId](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingStartGenerateStandbyScreenResource](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingStopGenerateStandbyScreenResource](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingRegisterCallback](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingUnregisterCallback](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingCheckCallback](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingSetStandbyScreenResource](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingGetCurrentBroadcastScreenLayout](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingLaunchLiveViewer](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingLaunchLiveViewerA](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingSetGuardAreas](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingStartSocialFeedbackMessageFiltering](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingStopSocialFeedbackMessageFiltering](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingApplySocialFeedbackMessageFilter](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingSetLinkCommentPreset](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingStartDebugBroadcast](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
- [sceGameLiveStreamingStopDebugBroadcast](../../../core/libs/prx/libSceGameLiveStreaming/Export.cpp) (libSceGameLiveStreaming) - unknown signature
