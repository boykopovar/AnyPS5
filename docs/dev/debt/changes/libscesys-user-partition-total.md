# libSceSystemService

## add Unknown function info

- [sceShellCoreUtilGetEffectiveTotalSizeOfUserPartition](../../../core/libs/prx/libSceSystemService/PendingExports.cpp) (libSceSystemService) - stays on `NotImplemented_nid_no_patch`: the only host source is the total the sibling `sceShellCoreUtilGetFreeSizeOfUserPartition` reports as `totalSize`, and "effective" may mean a different value (usable space after a reserve) with units and partition coverage unmeasured. A payload process on a PS5 (firmware 11.60) returned a non-zero total from the sibling; this export itself was not measured, so assuming it equals that total would pin an unmeasured value. Closes when: a console measurement or a firmware disassembly of this export shows the value and its relation to the sibling's total.
