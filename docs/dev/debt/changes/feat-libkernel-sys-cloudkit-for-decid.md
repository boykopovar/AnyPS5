# libkernel_sys

## remove `sceKernelIsCloudkitForDecid`

## add Functional

- [sceKernelIsCloudkitForDecid](../../../core/libs/prx/libkernel_sys/AdditionalExports.cpp) (libkernel_sys) returns 0, extending the retail Prospero model of the [console identity](../../../core/libs/prx/libkernel_sys/Export.cpp) queries: the host is not a cloud streaming kit, as `sceKernelIsCEX` answers 1. It reads no arguments, so its declaration stays parameterless. Console values are not measured on any model
