# libSceNpUtility

## change [AllocateRequestId]

- [AllocateRequestId](../../../core/libs/prx/libSceNpUtility/Export.cpp): Added a `0x1000` live-request host safety cap (returning `SCE_NP_COMMUNITY_ERROR_TOO_MANY_OBJECTS`) and `1..0x7FFFFFFF` live-ID-skipping wrap-around to `AppLaunchLinkCreateRequest` (`sceNpAppLaunchLinkIntCreateRequest`, `sceNpAppLaunchLink2IntCreateRequest`) and `sceNpServiceCheckerIntCreateRequest`, matching `sceNpServiceChecker2IntCreateRequest`.
