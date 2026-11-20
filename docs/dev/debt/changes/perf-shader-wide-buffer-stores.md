# shader

## add Functional

- [Wide buffer stores](../../../core/shader/recompiler/SpirvBackend/src/SpirvRuntimeBuffer.cpp) (shader recompiler) - devices whose `minStorageBufferOffsetAlignment` is not a multiple of 16 (AMD reports 4) keep per-component stores, as do `buffer_store_dwordx3`, typed 32_32_32, coherent, address-based, graphics-stage and swizzled-V# stores; loads are unchanged. Elsewhere a compute store of 8 or 16 bytes, or of a typed 4-byte record of several components, is one `u64`, `uvec4` or dword store through an aliased view when the V# base is dword aligned, the address is aligned to the store size, every component passes the V# bounds and the record is inside the descriptor range; other lanes run the per-component path, which stays in the module. No test makes the descriptor range shorter than the V# bounds. `APS5_NO_WIDE_BUFFER_STORES` clears the flag, which is in the shader cache key. Closes when: loads use the wide views.
