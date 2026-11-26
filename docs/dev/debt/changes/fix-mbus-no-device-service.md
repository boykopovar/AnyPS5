# libSceMbus

## add Unknown function info

- [sceMbus service calls](../../../core/libs/prx/libSceMbus/PendingExports.cpp) (libSceMbus) - a confirmed-signature call that needs the device service returns `0x809B00FF`, the same code `sceMbusAcquireControlList` returns, and does not read or write a service record. A record query with a null record returns `0x809B0080`. `sceMbusIsUsingDevice` returns 0 because no handle is registered. Shell BGM starts allowed; `sceMbusDisableBgmForShellUi` sets the flag, `sceMbusReenableBgmForShellUi` clears it, and `sceMbusIsBgmProhibited` returns 1 or 0. `sceMbusSetUsbPowerBlackList` rejects a count above 100, or a null list when the count is not 0, with `0x809B0080`. Parameterless placeholders still throw. Console results for these calls are unmeasured.

## add Silent stubs

- [sceMbus zero-argument dumps and audio wrappers](../../../core/libs/prx/libSceMbus/PendingExports.cpp) (libSceMbus) - `sceMbusDumpAppInfo`, `sceMbusDumpDeviceInfo`, `sceMbusDumpModuleStateInfoAll`, `sceMbusDumpPlayerInfo`, `sceMbusDumpPortHandleInfo`, `sceMbusDumpProcMediaInfoAll`, `sceMbusDumpUserMediaInfo`, `sceMbusRemoveCameraAppModuleFocus`, `sceMbusStartAudioOutAuxBroadcast`, `sceMbusStopAudioOutAuxBroadcast` and `sceMbusStopAudioOutSharePlay` return 0. Their zero-argument ABIs are confirmed and there is no device output or broadcast to change. `sceMbusStartAudioOutSharePlay` still takes a mode and returns `0x809B00FF`.
