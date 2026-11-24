# libkernel

## add Functional

- [AMM PRT pages on the GPU](../../../core/libs/prx/libkernel/DirectMemory/DirectMemory.cpp) (libkernel) - unbacked PRT pages (`DoMapZero`) are registered without the GPU bits, so the AGC driver leaves them out of its leases, buffer-address tables and host imports: a shader that reads one through a buffer address stops with `BDA access failed` instead of reading zeros. A `sceKernelMprotect` or `sceKernelMtypeprotect` that gives them a GPU bit registers them for the GPU again. Closes when: the driver serves zero pages as zeros without copying them
