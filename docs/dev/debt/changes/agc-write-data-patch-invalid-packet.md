# libSceAgc

## add Unknown function info

- [sceAgcWriteDataPatchSetAddressOrOffset](../../../core/libs/prx/libSceAgc/Patch/src/WaitRegMem.cpp) (libSceAgc, NID `fPSCdQxgpSw`) - not measured on a console. A packet whose opcode is not WRITE_DATA (0x37) returns 0x8a6c000c and is left unchanged, before the address is checked, as [KytyPS5](https://github.com/KytyPS5/KytyPS5/blob/87f59df17ae75b6303cfdf7a5d85b3d3ed664666/src/libs/agc.cpp#L1470-L1481) does; it is the code the PS5 12.70 module returns for a packet of the wrong type in `sceAgcAcquireMemSetEngine`. PPSA24913 patches a WRITE_DATA packet copied into a static scratch command buffer that another of its threads fills with `sceAgcDcbContextStateOp` packets, so the patch can land on a NOP. The Async, `SetDst` and `SetCachePolicy` forms still throw for such a packet, as no source covers them. Closes when: measured on a PS5.
