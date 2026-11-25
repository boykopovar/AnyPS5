# libSceAgc

## change sceAgcAsyncCondExecPatchSetCommandAddress, sceAgcAsyncCondExecPatchSetEnd
- [sceAgcCondExecPatchSetCommandAddress, sceAgcCondExecPatchSetEnd, sceAgcAsyncCondExecPatchSetCommandAddress and sceAgcAsyncCondExecPatchSetEnd](../../../core/libs/prx/libSceAgc/Patch/src/CondExec.cpp) (libSceAgc) - match `libSceAgc.sprx` (`0xcea0`-`0xcf9d`, 13.60): reject null or non-`0x22` packets with `0x8a6c000c` and mask command address (`~3`) and `end` dword count (`& 0x3fff`) instead of throwing via `ValidatePacket`; console runtime execution is unmeasured.
