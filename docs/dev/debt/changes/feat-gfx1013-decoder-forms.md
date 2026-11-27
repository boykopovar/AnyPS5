# shader

## add Functional

- [gfx1013-only memory forms](../../../core/shader/recompiler/RdnaDecoder/src/RdnaMemoryOpDecoder.cpp) (shader recompiler) - the scratch layout is not implemented, so the three `S_SCRATCH_STORE_*` forms translate to a throw. The other 21 DS SRC2, global store and buffer store forms and the 52 SMEM atomic forms (`S_ATOMIC_*`, `S_BUFFER_ATOMIC_*`) translate into the shared atomics and stores. An SMEM atomic takes its source data from the SDST field when GLC is 0 (LLVM `SMInstructions.td`), and GLC 1 names the destination with that same field because the pseudo has `Constraints = "$sdst = $sdata"`, so the old value returns into the source register; compare-swap reads the compare pair after the data. Stores and atomics take a wave-uniform predicate. The 64-bit SRC2 forms keep the 32-bit DS `Offset` scale (`DSInstructions.td` `DS_1A`), which is not measured on hardware. Closes when: the scratch stores are translated.
