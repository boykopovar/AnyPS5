# libSceAgc

## add Functional

- [sceAgcAcbResetQueue](../../../core/libs/prx/libSceAgc/Acb/src/Control.cpp) only models the unregistered caller-code range and the compatibility module's two default-context instances. The pinned console oracle's instance 2 still throws; its retail bounds/state are unknown. The source is static PPSA06652 compatibility code, not authenticated firmware or new console measurement. Active selection before initialization throws instead of that module's null dereference; a missing growth callback returns null instead of faulting. Invalid host pointers, concurrent buffer writes, heap failure and alternate-profile shader defaults are unmeasured. Immutable reset backing is retained for late title calls during shutdown.
