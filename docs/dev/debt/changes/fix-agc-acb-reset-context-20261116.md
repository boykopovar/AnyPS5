# libSceAgc

## add Functional

- [sceAgcAcbResetQueue](../../../core/libs/prx/libSceAgc/Acb/src/Control.cpp) follows the PPSA06652 compatibility module's two initialized default-context descriptors, low-16-bit flags, marker envelope, compute-SH restore, event and cache stages, and stage-specific growth results. Immutable reset backing is retained for late title calls during process shutdown. This is static analysis, not authenticated firmware or a new console measurement. Only the default unregistered caller-code range is modelled; configuring a different range remains unimplemented. Descriptor indices outside 0/1 and active-context selection before initialization throw instead of reading unrelated module memory or dereferencing null. A missing growth callback returns null instead of faulting. Invalid host pointers, concurrent mutation of one command buffer, allocation failure in default-state construction and the alternate profile's shader defaults are unmeasured.
