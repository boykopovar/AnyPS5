# libSceSystemService

## add Functional

- [sceShellCoreUtilGetEffectiveTotalSizeOfUserPartition](../../../core/libs/prx/libSceSystemService/PendingExports.cpp) (libSceSystemService) - returns the host filesystem's total capacity for the working directory, the same value `sceShellCoreUtilGetFreeSizeOfUserPartition` reports as its total because the two name one partition. A payload process on a PS5 (firmware 11.60) returned a non-zero total from the sibling, but the exact value, the units and what the partition covers are not measured. A null output is not written and returns 0; an unwritable non-null output returns `0x80E30005`, preserving `errno`. Neither output case was measured on a console.
