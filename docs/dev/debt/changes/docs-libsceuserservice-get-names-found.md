# libSceUserService

## change [sceUserServiceGetAppearOfflineSetting]
- [sceUserServiceGetAppearOfflineSetting](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsAccessTokenUstream]
- [sceUserServiceGetGlsAccessTokenUstream](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetIsHrtfEnabled]
- [sceUserServiceGetIsHrtfEnabled](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetCurrentUserGroupIndex]
- [sceUserServiceGetCurrentUserGroupIndex](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - 13.60 ABI is `(int32_t* index)` and dispatches user group query `0x30018`; AnyPS5 reads the current index from offset `0x44` in `/system_data/priv/home/user_list.dat`, matching the SceShellCore 13.60 handler logic. AnyPS5 does not create the global user list or numbered group files: on a fresh installation this call fails with the kernel open error or reports zero groups, until those files are provided. An unwritable non-null index returns `USER_SERVICE_ERROR_INVALID_ARGUMENT` as a host-safety fallback; console behavior for that pointer is unmeasured. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called.
## change [sceUserServiceGetPbtcThursdayHoursEnd]
- [sceUserServiceGetPbtcThursdayHoursEnd](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPbtcFridayHoursStart]
- [sceUserServiceGetPbtcFridayHoursStart](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetAudioFocus]
- [sceUserServiceGetAudioFocus](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsCameraTransparency]
- [sceUserServiceGetGlsCameraTransparency](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetUserStatus]
- [sceUserServiceGetUserStatus](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetHmd2MuteAssign]
- [sceUserServiceGetHmd2MuteAssign](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetIsCronosEnabled]
- [sceUserServiceGetIsCronosEnabled](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetSharePlayFramerateHost]
- [sceUserServiceGetSharePlayFramerateHost](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetCaptureStatus2]
- [sceUserServiceGetCaptureStatus2](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsCameraContrast]
- [sceUserServiceGetGlsCameraContrast](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPbtcWednesdayDuration]
- [sceUserServiceGetPbtcWednesdayDuration](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetSharePlayResolutionHost]
- [sceUserServiceGetSharePlayResolutionHost](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPsnPasswordForDebug]
- [sceUserServiceGetPsnPasswordForDebug](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPbtcAdditionalTime]
- [sceUserServiceGetPbtcAdditionalTime](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetNpLoginId]
- [sceUserServiceGetNpLoginId](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPadSpeakerVolume]
- [sceUserServiceGetPadSpeakerVolume](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsHintFlag]
- [sceUserServiceGetGlsHintFlag](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetAccessibilityKeyremapDataAston]
- [sceUserServiceGetAccessibilityKeyremapDataAston](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetParentalDvdRegion]
- [sceUserServiceGetParentalDvdRegion](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetParentalContentControl]
- [sceUserServiceGetParentalContentControl](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetVoiceAgentLanguage]
- [sceUserServiceGetVoiceAgentLanguage](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsAccessTokenNiconicoLive]
- [sceUserServiceGetGlsAccessTokenNiconicoLive](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetAppSortOrder]
- [sceUserServiceGetAppSortOrder](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsCameraBgFilter]
- [sceUserServiceGetGlsCameraBgFilter](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetHmd2GazeTarget]
- [sceUserServiceGetHmd2GazeTarget](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsMessageFilterLevel]
- [sceUserServiceGetGlsMessageFilterLevel](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetParentalDvd]
- [sceUserServiceGetParentalDvd](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPbtcTuesdayHoursStart]
- [sceUserServiceGetPbtcTuesdayHoursStart](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetVoiceRecognitionTutorialState]
- [sceUserServiceGetVoiceRecognitionTutorialState](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetFriendFlag]
- [sceUserServiceGetFriendFlag](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetEventCalendarType]
- [sceUserServiceGetEventCalendarType](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetIPDLeft]
- [sceUserServiceGetIPDLeft](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetHmd2CinematicAudioRotation]
- [sceUserServiceGetHmd2CinematicAudioRotation](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPbtcThursdayDuration]
- [sceUserServiceGetPbtcThursdayDuration](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPbtcSundayHoursStart]
- [sceUserServiceGetPbtcSundayHoursStart](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsIsFacebookEnabled]
- [sceUserServiceGetGlsIsFacebookEnabled](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetVoiceAgentEnableFeedback]
- [sceUserServiceGetVoiceAgentEnableFeedback](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetSharePlayFlags]
- [sceUserServiceGetSharePlayFlags](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetHearingAssistUsePreset]
- [sceUserServiceGetHearingAssistUsePreset](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetA11yControllerLedBrightness]
- [sceUserServiceGetA11yControllerLedBrightness](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetKeyRepeatStartingTime]
- [sceUserServiceGetKeyRepeatStartingTime](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetProControllerAlreadyNoticed]
- [sceUserServiceGetProControllerAlreadyNoticed](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetNpAge]
- [sceUserServiceGetNpAge](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPlayTogetherFlags]
- [sceUserServiceGetPlayTogetherFlags](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetHmd2HideCameraImage]
- [sceUserServiceGetHmd2HideCameraImage](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsAccessTokenTwitch]
- [sceUserServiceGetGlsAccessTokenTwitch](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsLiveQuality2]
- [sceUserServiceGetGlsLiveQuality2](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetSparkEqParam]
- [sceUserServiceGetSparkEqParam](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPbtcMondayHoursEnd]
- [sceUserServiceGetPbtcMondayHoursEnd](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetIsFakePlus]
- [sceUserServiceGetIsFakePlus](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPbtcTuesdayHoursEnd]
- [sceUserServiceGetPbtcTuesdayHoursEnd](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPartyMuteList]
- [sceUserServiceGetPartyMuteList](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetNpNpId]
- [sceUserServiceGetNpNpId](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetProControllerBtAddress]
- [sceUserServiceGetProControllerBtAddress](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetSparkEqIndex]
- [sceUserServiceGetSparkEqIndex](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsCommunityId]
- [sceUserServiceGetGlsCommunityId](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGazeCalibData]
- [sceUserServiceGetGazeCalibData](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetHearingAssistUseAudiogramData]
- [sceUserServiceGetHearingAssistUseAudiogramData](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPbtcFridayDuration]
- [sceUserServiceGetPbtcFridayDuration](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetProControllerUniqueId1]
- [sceUserServiceGetProControllerUniqueId1](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetEventSortEvent]
- [sceUserServiceGetEventSortEvent](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsLfpsSortOrder]
- [sceUserServiceGetGlsLfpsSortOrder](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetHearingAssistMakeVoicesClearer]
- [sceUserServiceGetHearingAssistMakeVoicesClearer](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetVoiceAgentEnableByButton]
- [sceUserServiceGetVoiceAgentEnableByButton](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetSystemLoggerHashedAccountIdClockType]
- [sceUserServiceGetSystemLoggerHashedAccountIdClockType](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetCaptureStatus]
- [sceUserServiceGetCaptureStatus](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetHmuZoom]
- [sceUserServiceGetHmuZoom](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsBcTitle]
- [sceUserServiceGetGlsBcTitle](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPbtcFridayHoursEnd]
- [sceUserServiceGetPbtcFridayHoursEnd](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetLoginFlag]
- [sceUserServiceGetLoginFlag](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetHearingAssistAdjustmentLevelLeft]
- [sceUserServiceGetHearingAssistAdjustmentLevelLeft](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetTriggerEffectStrength]
- [sceUserServiceGetTriggerEffectStrength](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPbtcSaturdayHoursStart]
- [sceUserServiceGetPbtcSaturdayHoursStart](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetVoiceAgentSessionTimeout]
- [sceUserServiceGetVoiceAgentSessionTimeout](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPartyMuteListA]
- [sceUserServiceGetPartyMuteListA](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsCameraEliminationLevel]
- [sceUserServiceGetGlsCameraEliminationLevel](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsBroadcastersCommentColor]
- [sceUserServiceGetGlsBroadcastersCommentColor](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetParentalBrowser]
- [sceUserServiceGetParentalBrowser](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetProControllerUniqueId2]
- [sceUserServiceGetProControllerUniqueId2](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsCameraEdgeLevel]
- [sceUserServiceGetGlsCameraEdgeLevel](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsCameraPosition]
- [sceUserServiceGetGlsCameraPosition](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetA11yControllerAName]
- [sceUserServiceGetA11yControllerAName](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetNotificationSettings]
- [sceUserServiceGetNotificationSettings](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetTopMenuTutorialFlag]
- [sceUserServiceGetTopMenuTutorialFlag](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPbtcSaturdayHoursEnd]
- [sceUserServiceGetPbtcSaturdayHoursEnd](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetImeLastUnit]
- [sceUserServiceGetImeLastUnit](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsIsRecievedMessageHidden]
- [sceUserServiceGetGlsIsRecievedMessageHidden](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetHmd2Vibration]
- [sceUserServiceGetHmd2Vibration](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPbtcMondayHoursStart]
- [sceUserServiceGetPbtcMondayHoursStart](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetIsQuickSignup]
- [sceUserServiceGetIsQuickSignup](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsCameraDepthLevel]
- [sceUserServiceGetGlsCameraDepthLevel](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetHmuBrightness]
- [sceUserServiceGetHmuBrightness](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsTtsVolume]
- [sceUserServiceGetGlsTtsVolume](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetMicLevel]
- [sceUserServiceGetMicLevel](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetSaveDataAutoUpload]
- [sceUserServiceGetSaveDataAutoUpload](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetVolumeForSidetone]
- [sceUserServiceGetVolumeForSidetone](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetFriendCustomListLastFocus]
- [sceUserServiceGetFriendCustomListLastFocus](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetKeyRepeatSpeed]
- [sceUserServiceGetKeyRepeatSpeed](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetImePointerMode]
- [sceUserServiceGetImePointerMode](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetVolumeForController]
- [sceUserServiceGetVolumeForController](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsBroadcastChannel]
- [sceUserServiceGetGlsBroadcastChannel](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetSystemLoggerHashedAccountIdParam]
- [sceUserServiceGetSystemLoggerHashedAccountIdParam](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetHintShowFlag2]
- [sceUserServiceGetHintShowFlag2](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetNotificationSettings_3]
- [sceUserServiceGetNotificationSettings_3](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsAnonymousUserId]
- [sceUserServiceGetGlsAnonymousUserId](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetVoiceAgentEnableWup]
- [sceUserServiceGetVoiceAgentEnableWup](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetNpDateOfBirth]
- [sceUserServiceGetNpDateOfBirth](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetHearingAssistSeparateLR]
- [sceUserServiceGetHearingAssistSeparateLR](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetVoiceAgentTtsVolume]
- [sceUserServiceGetVoiceAgentTtsVolume](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPbtcMondayDuration]
- [sceUserServiceGetPbtcMondayDuration](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetNpSubAccount]
- [sceUserServiceGetNpSubAccount](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetShareStatus2]
- [sceUserServiceGetShareStatus2](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsCamCrop]
- [sceUserServiceGetGlsCamCrop](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetVoiceRecognitionLastUsedOsk]
- [sceUserServiceGetVoiceRecognitionLastUsedOsk](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetSystemLoggerHashedAccountId]
- [sceUserServiceGetSystemLoggerHashedAccountId](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsLiveQuality5]
- [sceUserServiceGetGlsLiveQuality5](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPbtcFlag]
- [sceUserServiceGetPbtcFlag](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetIPDRight]
- [sceUserServiceGetIPDRight](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetNotificationBehavior]
- [sceUserServiceGetNotificationBehavior](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetHearingAssistAudiogramData]
- [sceUserServiceGetHearingAssistAudiogramData](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetHintShowFlag]
- [sceUserServiceGetHintShowFlag](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetParentalBdAge]
- [sceUserServiceGetParentalBdAge](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetSaveDataSort]
- [sceUserServiceGetSaveDataSort](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetHrtf]
- [sceUserServiceGetHrtf](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsStreamingMode]
- [sceUserServiceGetGlsStreamingMode](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetChatStatus]
- [sceUserServiceGetChatStatus](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetNpCountryCode]
- [sceUserServiceGetNpCountryCode](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetParentalGameWhiteList]
- [sceUserServiceGetParentalGameWhiteList](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetTriggerEffectEnabled]
- [sceUserServiceGetTriggerEffectEnabled](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetA11yControllerInitialSetupProgress]
- [sceUserServiceGetA11yControllerInitialSetupProgress](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetVolumeForGenericUSB]
- [sceUserServiceGetVolumeForGenericUSB](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetControlCenterFunctionControlSettings]
- [sceUserServiceGetControlCenterFunctionControlSettings](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsIsMuteEnabled]
- [sceUserServiceGetGlsIsMuteEnabled](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetNpLanguageCode2]
- [sceUserServiceGetNpLanguageCode2](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetControlCenterStatus]
- [sceUserServiceGetControlCenterStatus](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetSparkEqProfile]
- [sceUserServiceGetSparkEqProfile](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsTtsPitch]
- [sceUserServiceGetGlsTtsPitch](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetAutoLoginEnabled]
- [sceUserServiceGetAutoLoginEnabled](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetNotificationSettings_1]
- [sceUserServiceGetNotificationSettings_1](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPasscode]
- [sceUserServiceGetPasscode](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsBroadcastUiLayout]
- [sceUserServiceGetGlsBroadcastUiLayout](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetAccountRemarks]
- [sceUserServiceGetAccountRemarks](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetSystemLoggerHashedAccountIdTtl]
- [sceUserServiceGetSystemLoggerHashedAccountIdTtl](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsTtsSpeed]
- [sceUserServiceGetGlsTtsSpeed](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetNpEnv]
- [sceUserServiceGetNpEnv](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsCameraSize]
- [sceUserServiceGetGlsCameraSize](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetDiscPlayerFlag]
- [sceUserServiceGetDiscPlayerFlag](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetEventFilterTeamEvent]
- [sceUserServiceGetEventFilterTeamEvent](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetNpOfflineAccountAdult]
- [sceUserServiceGetNpOfflineAccountAdult](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetImeInputType]
- [sceUserServiceGetImeInputType](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetVolumeForMorpheusSidetone]
- [sceUserServiceGetVolumeForMorpheusSidetone](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetHearingAssistPresetType]
- [sceUserServiceGetHearingAssistPresetType](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetHearingAssistWeakenAdjustment]
- [sceUserServiceGetHearingAssistWeakenAdjustment](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPadProfileDataHuntList]
- [sceUserServiceGetPadProfileDataHuntList](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetAccessibilityKeyremapData]
- [sceUserServiceGetAccessibilityKeyremapData](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetVoiceAgentTtsSpeed]
- [sceUserServiceGetVoiceAgentTtsSpeed](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPbtcWednesdayHoursEnd]
- [sceUserServiceGetPbtcWednesdayHoursEnd](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetAgeVerified]
- [sceUserServiceGetAgeVerified](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPartyChatTranscriptionLanguageType]
- [sceUserServiceGetPartyChatTranscriptionLanguageType](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPbtcTuesdayDuration]
- [sceUserServiceGetPbtcTuesdayDuration](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetNpOnlineId]
- [sceUserServiceGetNpOnlineId](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetHintShowFlag3]
- [sceUserServiceGetHintShowFlag3](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetCreatedVersion]
- [sceUserServiceGetCreatedVersion](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsCameraChromaKeyLevel]
- [sceUserServiceGetGlsCameraChromaKeyLevel](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetNpOfflineAccountId]
- [sceUserServiceGetNpOfflineAccountId](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetShareStatus3]
- [sceUserServiceGetShareStatus3](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetTeamShowAboutTeam]
- [sceUserServiceGetTeamShowAboutTeam](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetA11yControllerBName]
- [sceUserServiceGetA11yControllerBName](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPlayingBigAppTitleId]
- [sceUserServiceGetPlayingBigAppTitleId](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetSaveDataAutoSync]
- [sceUserServiceGetSaveDataAutoSync](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetHmd2Brightness]
- [sceUserServiceGetHmd2Brightness](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetHoldAudioOutDevice]
- [sceUserServiceGetHoldAudioOutDevice](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetTopMenuNotificationFlag]
- [sceUserServiceGetTopMenuNotificationFlag](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPbtcSundayDuration]
- [sceUserServiceGetPbtcSundayDuration](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPartyChatTranscriptionFlag]
- [sceUserServiceGetPartyChatTranscriptionFlag](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPbtcSaturdayDuration]
- [sceUserServiceGetPbtcSaturdayDuration](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPartySettingFlags]
- [sceUserServiceGetPartySettingFlags](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetImeInputSettings]
- [sceUserServiceGetImeInputSettings](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetA11yControllerBDeviceUniqueIds]
- [sceUserServiceGetA11yControllerBDeviceUniqueIds](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetVibrationStrength]
- [sceUserServiceGetVibrationStrength](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetNpAccountUpgradeFlag]
- [sceUserServiceGetNpAccountUpgradeFlag](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetDeletedUserInfo]
- [sceUserServiceGetDeletedUserInfo](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetHearingAssistAdjustmentLevelRight]
- [sceUserServiceGetHearingAssistAdjustmentLevelRight](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPlatformPrivacyWs1Internal]
- [sceUserServiceGetPlatformPrivacyWs1Internal](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsIsRecDisabled]
- [sceUserServiceGetGlsIsRecDisabled](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsCameraBrightness]
- [sceUserServiceGetGlsCameraBrightness](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPbtcThursdayHoursStart]
- [sceUserServiceGetPbtcThursdayHoursStart](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsBroadcastService]
- [sceUserServiceGetGlsBroadcastService](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetIsFakePremium]
- [sceUserServiceGetIsFakePremium](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetSaveDataTutorialFlag]
- [sceUserServiceGetSaveDataTutorialFlag](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsLiveQuality4]
- [sceUserServiceGetGlsLiveQuality4](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetFaceRecognitionDeleteCount]
- [sceUserServiceGetFaceRecognitionDeleteCount](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetVoiceAgentEnable]
- [sceUserServiceGetVoiceAgentEnable](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsTtsFlags]
- [sceUserServiceGetGlsTtsFlags](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsBroadcastersComment]
- [sceUserServiceGetGlsBroadcastersComment](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPbtcTzOffset]
- [sceUserServiceGetPbtcTzOffset](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPbtcPlayTime]
- [sceUserServiceGetPbtcPlayTime](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetVoiceAgentStatusBit]
- [sceUserServiceGetVoiceAgentStatusBit](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetNotificationSettings2]
- [sceUserServiceGetNotificationSettings2](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetEventUiFlag]
- [sceUserServiceGetEventUiFlag](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetMonoOutput]
- [sceUserServiceGetMonoOutput](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetShareStatus]
- [sceUserServiceGetShareStatus](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetHmd2GazeActivate]
- [sceUserServiceGetHmd2GazeActivate](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetUserGroupNameList]
- [sceUserServiceGetUserGroupNameList](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - 13.60 ABI is `(int groupIndex, char* nameList, size_t size, int32_t* count)` and dispatches query `0x3001a`; it reads up to 16 IDs from the group file (or the global list for index `-1`) and writes fixed 17-byte name slots. AnyPS5 resolves IDs supported by its user-name model; unsupported IDs return `USER_SERVICE_ERROR_UNKNOWN_USER` because shell user configuration is not modeled. The global and numbered user-list files are not created by AnyPS5; a fresh installation returns the kernel open error, including for index -1. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called.
## change [sceUserServiceGetGazeActivate]
- [sceUserServiceGetGazeActivate](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsIsTwitterEnabled]
- [sceUserServiceGetGlsIsTwitterEnabled](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetTopMenuLimitItem]
- [sceUserServiceGetTopMenuLimitItem](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetNpAuthErrorFlag]
- [sceUserServiceGetNpAuthErrorFlag](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsIsCameraHidden]
- [sceUserServiceGetGlsIsCameraHidden](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetMousePointerSpeed]
- [sceUserServiceGetMousePointerSpeed](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetHmd2PlayAreaShape]
- [sceUserServiceGetHmd2PlayAreaShape](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetA11yControllerADeviceUniqueIds]
- [sceUserServiceGetA11yControllerADeviceUniqueIds](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsFloatingMessage]
- [sceUserServiceGetGlsFloatingMessage](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsBcTags]
- [sceUserServiceGetGlsBcTags](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPbtcMode]
- [sceUserServiceGetPbtcMode](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetProControllerUniqueId3]
- [sceUserServiceGetProControllerUniqueId3](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetCaptureStatus3]
- [sceUserServiceGetCaptureStatus3](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsCameraReflection]
- [sceUserServiceGetGlsCameraReflection](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetNotificationSettings_2]
- [sceUserServiceGetNotificationSettings_2](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsLiveQuality]
- [sceUserServiceGetGlsLiveQuality](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetFaceRecognitionRegisterCount]
- [sceUserServiceGetFaceRecognitionRegisterCount](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetIsRemotePlayAllowed]
- [sceUserServiceGetIsRemotePlayAllowed](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsLiveQuality3]
- [sceUserServiceGetGlsLiveQuality3](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPsCloudFeedbackTime]
- [sceUserServiceGetPsCloudFeedbackTime](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPbtcWednesdayHoursStart]
- [sceUserServiceGetPbtcWednesdayHoursStart](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetIsPersonalizedHrtfEnabled]
- [sceUserServiceGetIsPersonalizedHrtfEnabled](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetAccessibilityKeyremapEnableAston]
- [sceUserServiceGetAccessibilityKeyremapEnableAston](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetHmd2ScreenSize]
- [sceUserServiceGetHmd2ScreenSize](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsInitSpectating]
- [sceUserServiceGetGlsInitSpectating](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetLastLoginOrder]
- [sceUserServiceGetLastLoginOrder](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetMouseHandType]
- [sceUserServiceGetMouseHandType](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetPbtcPlayTimeLastUpdated]
- [sceUserServiceGetPbtcPlayTimeLastUpdated](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetParentalMorpheus]
- [sceUserServiceGetParentalMorpheus](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsOverlayPosition]
- [sceUserServiceGetGlsOverlayPosition](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetEventSortTitle]
- [sceUserServiceGetEventSortTitle](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetGlsLanguageFilter]
- [sceUserServiceGetGlsLanguageFilter](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetVibrationEnabled]
- [sceUserServiceGetVibrationEnabled](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetLightBarBaseBrightness]
- [sceUserServiceGetLightBarBaseBrightness](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetVoiceAgentStatusBitFactory]
- [sceUserServiceGetVoiceAgentStatusBitFactory](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetParentalGame]
- [sceUserServiceGetParentalGame](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetParentalGameAgeLevel]
- [sceUserServiceGetParentalGameAgeLevel](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - unknown signature. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceUserServiceGetUserGroupNum]
- [sceUserServiceGetUserGroupNum](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - 13.60 ABI is `(int32_t* count)` and dispatches query `0x30017`; AnyPS5 scans `/system_data/priv/home/user_list.dat.<index>` from slot 0 and counts consecutive existing slots up to 128, matching the SceShellCore 13.60 handler logic. AnyPS5 does not create the global user list or numbered group files: on a fresh installation this call fails with the kernel open error or reports zero groups, until those files are provided. An unwritable non-null count returns `USER_SERVICE_ERROR_INVALID_ARGUMENT` as a host-safety fallback; console behavior for that pointer is unmeasured. Found by `dlsym` in `libSceUserService.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called.
