# shader

## add Unknown function info

- [GCN V# with DATA_FORMAT INVALID](../../../core/shader/recompiler/SpirvBackend/src/SpirvRuntimeBuffer.cpp) (shader, PS4) - `buffer_load_format_*` through a V# whose DATA_FORMAT is 0 returns zeros and `buffer_store_format_*` writes nothing when the V# comes from user data or another snapshot source ([DescriptorBindingBuilder](../../../core/shader/recompiler/Optimization/src/DescriptorBindingBuilder.cpp), pinned in [GcnInvalidBufferFormat.cpp](../../../core/libs/prx/libSceAgcDriver/tests/execution/GcnInvalidBufferFormat.cpp)). The GCN3 ISA guide (rev 1.1, section 8.1.4) gives that format the meaning "unbound resource", and its table 8.9 note says an unbound buffer returns 0,0,0,0. A V# the shader loads from memory at run time still records an `InvalidDescriptor` fault and stops the invocation, and a formatted access whose V# word 3 is a constant with that format still throws. Not measured on a PS4; the PS5's gfx1013 is not checked.
