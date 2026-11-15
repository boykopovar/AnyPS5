# shader

## change [Buffer descriptor type]

- [Buffer descriptor type](../../../core/shader/recompiler/Optimization/src/ResourceMaterializer.cpp) - a buffer V# whose TYPE (word 3 bits 30-31) is 1, or is not 0 in a GCN shader, still throws `buffer descriptor uses an unsupported type`; so does TYPE 2 or 3 on a buffer the program writes, reads atomically or does not read. Measured on a PS5 (CFI-1115B, firmware 12.20): through TYPE 2 or 3 with FORMAT 0 or a valid format, `buffer_store_dword` and `buffer_store_format_x` wrote nothing and did not fault, also with an unmapped base; loads and GLC atomics left the destination VGPR unchanged, which a null V# can't reproduce. A vector load through TYPE 2 or 3 now keeps the destination: the load is emitted as a select that returns the value the destination register held before it, taken from the register the translation reads for the same instruction, and the buffer is bound as an empty buffer that nothing reads. Stores, atomics, scalar loads, `tbuffer_*`, FORMAT above 77, GCN shaders and V#s loaded at run time still throw.

## add Silent stubs

- [Buffer loads through a discarded V#](../../../core/shader/recompiler/SpirvBackend/src/SpirvRuntimeBuffer.cpp) - the preserved value is the destination register's previous SSA value read at translation time; a shader that reads the same destination again after the load sees that value, as the console leaves it, but no title has been measured depending on the exact stale contents, and a load whose destination register has no earlier value in the program keeps whatever the recompiler gives an undefined register.
