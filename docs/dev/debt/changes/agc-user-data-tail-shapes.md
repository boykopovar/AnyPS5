# shader

## change [Vertex user-data tail jumps]

- [Vertex user-data tail jumps](../../../core/shader/recompiler/ControlFlow/src/GraphBuilder.cpp) link a raw vertex prolog's `s_setpc_b64` through an unchanged user-data pair to its continuation, also when the prolog runs as the front of a geometry draw (mesh stage). The instruction semantics follow AMD RDNA2 ISA section 12.3; the prolog convention follows Mesa `aco_select_vs_prolog.cpp` at `0b2d6d1a61d9bbf89cf8beb88a696144f67c61f8`. Console hardware has not been measured. The body may branch only to code after the `s_setpc_b64` that ends in `s_endpgm` and holds no branch, and the low half may be one `s_mov_b32` copy of unchanged user data when the high half is an immediate. Other branches, computed low pointers and continuations reading `s_getpc_b64` remain unsupported

## change [Raw fragment user-data tails]

- [Raw fragment user-data tails](../../../core/shader/recompiler/ControlFlow/src/GraphBuilder.cpp) also link a `s_setpc_b64` when the low half is unchanged user data, or one `s_mov_b32` copy of it, and the last high-half writer is an unconditional immediate `s_mov_b32`; as for vertex prologs, the body may branch to an `s_endpgm` exit after the jump. Other computed high halves remain unsupported. This follows AMD RDNA2 ISA section 12.3 and Mesa `aco_select_nir.cpp` (`create_fs_jump_to_epilog`), `aco_isel_helpers.cpp` (`convert_pointer_to_64_bit`) and `aco_lower_to_hw_instr.cpp` (`p_jump_to_epilog`) at `0b2d6d1a61d9bbf89cf8beb88a696144f67c61f8`; console hardware has not been measured
