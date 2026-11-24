# shader

## add Functional

- [Waves that end before a later barrier](../../../core/shader/recompiler/SpirvBackend/src/SpirvFlowEmitter.cpp) (shader recompiler) - in a compute program with more than one wave per workgroup, a branch to an `s_endpgm` that skips an `s_barrier` reached by the other waves does not leave: the wave runs the rest of the program with its LDS, GDS, buffer, image and memory writes masked, so every invocation reaches each `OpControlBarrier` (Intel's Windows driver hangs when a workgroup barrier is reached by only some invocations). The masked wave still runs loads, scalar work and wave operations there. Only an exit that is the merge of the branch's selection is handled; an exit taken from inside a loop, or reached through other blocks, still leaves early.
