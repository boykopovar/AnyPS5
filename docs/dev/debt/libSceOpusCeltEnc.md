# libSceOpusCeltEnc technical debt

### Unknown function info

- [sceOpusCeltEncCtl, sceOpusCeltEncEncodeFloatWithPriority and sceOpusCeltEncEncodeWithPriority](../../../core/libs/prx/libSceOpusCeltEnc/Export.cpp) (libSceOpusCeltEnc) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceOpusCeltEnc.c). The prototypes mirror the sibling declarations committed for the encoder inventory and libopus's own ctl and priority entry points, but the console's request codes, priority and batch semantics were not recovered. They throw through `NotImplemented_nid_no_patch`; this is not an ABI claim.

### Functional

- [libSceOpusCeltEnc](../../../core/libs/prx/libSceOpusCeltEnc/Export.cpp) uses bundled libopus 1.6.1 with `OPUS_APPLICATION_RESTRICTED_CELT` for mono/stereo signed-16 and float encoding at 8000, 12000, 16000, 24000 and 48000 Hz. `Create` and `CreateEx` validate channels before sample rate before application; the state pointer is only a registry key, nothing is written into guest memory

- [sceOpusCeltEncTerminate](../../../core/libs/prx/libSceOpusCeltEnc/Export.cpp) throws while a `CreateEx` encoder is live and `Create` without `CreateEx` binds an internal context id. The error base with the offsets 0, 1, 2, 3, 6, 7, 8, 9 and 13 follows the recovered libSceOpusCeltDec validation by analogy; no encoder measurement confirms it

- A zero-frame [sceOpusCeltEncEncode](../../../core/libs/prx/libSceOpusCeltEnc/Export.cpp) or [sceOpusCeltEncEncodeFloat](../../../core/libs/prx/libSceOpusCeltEnc/Export.cpp), a libopus encoding rejection and `OPUS_ALLOC_FAIL` throw. AJM allocation and failures, AJM context ownership, the console `GetSize` value and the console's packet equivalence remain unimplemented. Closes when: a supplied PS5 module settles the state layout behind `GetSize`, the error base with its validation offsets and the ctl request and priority behaviour of the throwing exports
