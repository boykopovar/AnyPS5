# libkernel_sys

## remove `sceKernelIsAuthenticTrinity`

## remove `sceKernelIsCloudkit`

## add Functional

- [sceKernelIsAuthenticTrinity and sceKernelIsCloudkit](../../../core/libs/prx/libkernel_sys/AdditionalExports.cpp) (libkernel_sys) return 0, extending the retail Prospero model of the [console identity](../../../core/libs/prx/libkernel_sys/Export.cpp) queries: the host is not PS5 Pro (Trinity) hardware, as `sceKernelHasTrinityMode` and `sceKernelIsTrinityMode` already answer 0, and not a cloud streaming kit, as `sceKernelIsCEX` answers 1. Neither reads an argument, so the declarations stay parameterless. [Jelly5](https://github.com/02dnot/Jelly5/blob/333e393b7f1a5b98d88099b4782dab97bd10fa5d/app/engine/media/src/evo_hw.c#L58-L68) resolves `sceKernelIsAuthenticTrinity` by NID `X0HkB92+NRE`, calls it without arguments, treats a set bit 31 as an error and its low byte as a boolean, and takes nonzero to mean PS5 Pro hardware; the value on a PS5 Pro, and the prototype of `sceKernelIsCloudkit`, are unmeasured. Console values are not measured on any model
