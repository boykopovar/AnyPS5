# libSceNpSessionSignalingCrossplay

## change [11 exports (6 without a known name, bound by NID)]

- [8 unimplemented exports (6 without a known name, bound by NID)](../../../core/libs/prx/libSceNpSessionSignalingCrossplay/Export.cpp) (libSceNpSessionSignalingCrossplay) - exports from the firmware 13.60 table other than Initialize, Terminate and GetMemoryInfo; signatures and behaviour are unknown apart from `wypjyjGWaLM` null checks and throw via `NotImplemented_nid_no_patch`

## change [sceNpSessionSignalingCrossplayInitialize, sceNpSessionSignalingCrossplayGetMemoryInfo and sceNpSessionSignalingCrossplayUnknown02 (`wypjyjGWaLM`)]

- [Initialize, Terminate, GetMemoryInfo and `wypjyjGWaLM`](../../../core/libs/prx/libSceNpSessionSignalingCrossplay/Export.cpp) (libSceNpSessionSignalingCrossplay) - from firmware 13.42 disassembly, not measured on console: Initialize uses `heapSize` and ignores the other 0x28-byte fields; `Unknown02` only checks nulls and throws
