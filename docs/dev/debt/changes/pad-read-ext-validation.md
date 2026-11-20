# libScePad

## change [scePadReadStateExt and scePadGetDeviceId]

- [scePadReadExt, scePadReadStateExt and scePadGetDeviceId](../../../core/libs/prx/libScePad/Export.cpp) (libScePad) - `ReadExt` and `ReadStateExt` validate initialisation, non-null outputs and handle bounds before throwing for unmodelled extended records. `ReadStateExt` checks a null output, then initialisation, then the handle; `ReadExt` checks initialisation, parameters (`data` non-null, `count` 1-64), then handle validity. `GetDeviceId(handle, int32*)` writes -1, then refuses an unknown handle with `0x80920003`.
