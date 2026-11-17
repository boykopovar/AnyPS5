# libSceAgcDriver

## add Functional

- [Target memo tests without the rewatch under images on Windows](../../../core/libs/prx/libSceAgcDriver/tests/TargetStateMemo.cpp) (libSceAgcDriver) - on Windows the target memo test binary sets `APS5_NO_REWATCH_UNDER_IMAGES=1`, so its tests do not run the default path where a settled host import under a live image returns to the write watch. With that path the rewatch lands inside a full lookup at a time the tests cannot control: the baseline fixtures lose their saved bytes, a repeated draw misses its memo hit and `APS5_VERIFY_TARGET_MEMO` reports image content moved by the lookup's own rewatch (#22159). Remove the switch from the test once the memo and the verify mode account for that rewatch.
