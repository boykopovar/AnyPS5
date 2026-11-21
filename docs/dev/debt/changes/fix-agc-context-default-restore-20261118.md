# libSceAgc

## change [sceAgcDcbContextStateOp]

- [sceAgcDcbContextStateOp and sceAgcDcbContextStateOp_0100](../../../core/libs/prx/libSceAgc/DcbState/src/ContextState.cpp) (libSceAgc) - nondefault shadow-mode initialization and `_0100` operations 1 to 3 remain incomplete. Unversioned operation 3 in the supported default mode emits CONTEXT_CONTROL, CLEAR_STATE push and an indexed default CX load, with independent packet growth. Prior writes survive failure; an incomplete sequence returns null. It captures defaults after the push and before load growth. This follows PPSA06652's bundled compatibility module with synthetic CPU buffers, not authenticated retail firmware or a fresh console measurement. Operation 0 restores initialized defaults for both entry points. Missing initialization or invalid host bounds throws; default backing is retained for process lifetime. The legacy size query and sceAgcInit platform/driver adjustment gaps remain.
