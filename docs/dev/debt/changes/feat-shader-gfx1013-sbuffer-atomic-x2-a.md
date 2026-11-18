# shader

## add Functional

- [S_BUFFER_ATOMIC_*_X2](../../../core/shader/recompiler/Translation/src/MemoryInstructions/ScalarBufferInstructions.cpp) (gfx1013) - the 64-bit scalar buffer atomics SWAP/CMPSWAP/ADD/SUB/SMIN/UMIN/SMAX (SMEM 0x60-0x66) are decoded and translated to the native-wide runtime V# atomics, with SDST holding data and the old value written back when `glc` is set. They are not measured on the PS5: offset scaling, add-TID bit behaviour, fault handling, contended lane ordering and VCC-base forms are unverified. Operations and operand order follow LLVM llvmorg-19.1.0 `SMInstructions.td` lines 1276-1340 and AMD's RDNA ISA SMEM table. The V# is built with buffer metadata and a zero lane index, so an add-TID bit in word 3 would wrongly apply a lane offset; that bit is clear for the descriptors used here. 64-bit atomics need `shaderBufferInt64Atomics` on the Vulkan device. Closes when: measured on a PS5.
