# shader

## change [GCN buffer and image descriptors]

- [GCN buffer and image descriptors](../../../core/shader/recompiler/RdnaDecoder/src/RdnaDescriptorFormat.cpp) (shader, PS4) - a GCN V# word 3 (DATA_FORMAT [18:15], NUM_FORMAT [14:12], names read from the most significant field) is rewritten to the unified format of the PS5 layout with OOB_SELECT 1 for a strided buffer and 3 otherwise, following the GFX6-7 NUM_RECORDS rules in Mesa's `ac_descriptors`; ATC, HASH, HEAP and MTYPE are dropped. The V# base is 44 bits, as in shadPS4's `AmdGpu::Buffer`: Limbo leaves 0x7 in word 1 bits [15:12]; the [descriptor walker](../../../core/shader/recompiler/Optimization/include/Optimization/SrtWalker/SrtAddressArithmetic.hpp) reads `s_buffer_load` through it too. A GCN T# is rewritten to the PS5 layout with SW_MODE linear, keeping TILING_INDEX [4:0], POW2_PAD [5] and PITCH - 1 [19:6] in word 7. None of it is checked on a console.
