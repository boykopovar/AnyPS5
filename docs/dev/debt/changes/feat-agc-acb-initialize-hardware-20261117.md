# libSceAgc

## remove sceAgcAcbInitializeDefaultHardwareState

## add Functional

- [sceAgcAcbInitializeDefaultHardwareState](../../../core/libs/prx/libSceAgc/Unimplemented.cpp) copies the active initialized context's compute-default packets, or returns null when none exist. Its signature and allocation/callback behavior come from static PPSA06652 compatibility code, not authenticated firmware or a new console measurement. Three published console cases cover only the uninitialized null return. Initialized versions 8, 10 and 13 are tested against the shared register tables; active selection, capacity failure and callback behavior lack console measurements. A null buffer after initialization throws and a missing growth callback returns null instead of reproducing the module's invalid dereference. Invalid host pointers and concurrent writes to the same buffer remain unmeasured.

# libSceAgcVsh

## change Two further firmware exports remain

- Two further firmware exports remain [unwired](../../../core/libs/prx/libSceAgcVsh/Export.cpp) and unnamed: `Xvcgh9xNNkY` (reporter-trap body in Vsh; `libSceAgc.sprx` serves a real body at `va=0x5280`) and `rPtseVo5ToI` (reporter-trap in Vsh; resolves by NID hash to `sceAgcAcbInitializeDefaultHardwareState`, which `libSceAgc.sprx` serves at `va=0x940` and [libSceAgc](../../../core/libs/prx/libSceAgc/Unimplemented.cpp) now provides as a separate compute-default initializer). All five NIDs are absent from the 3.20 export inventory that generated the [stub list](../../../core/libs/prx/libSceAgcVsh/Export.cpp), and `Aozdh0bCcO0`/`MGEQMJBhdtc`/`ddU6mOWyY40`/`Xvcgh9xNNkY` remain absent from aerolib.
