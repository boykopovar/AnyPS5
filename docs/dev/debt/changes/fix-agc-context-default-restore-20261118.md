# libSceAgc

## change [sceAgcDcbContextStateOp]

- [sceAgcDcbContextStateOp and sceAgcDcbContextStateOp_0100](../../../core/libs/prx/libSceAgc/DcbState/src/ContextState.cpp) (libSceAgc) - operation 0 emits a five-word indexed load of the current initialized context defaults, capturing the backing before a growth callback. The descriptor selection, packet layout and callback behavior follow PPSA06652's bundled compatibility module, not authenticated retail firmware or a fresh console measurement. Calling before initialization, or with invalid buffer bounds or alignment, throws as host validation instead of reproducing an invalid reference access. The selected backing is retained for process lifetime. Synchronization, shadow-mode and version differences in the other operations remain incomplete; the size query retains KytyPS5's reported sizes. Platform registration and driver-provided default adjustments remain incomplete as documented for sceAgcInit.
