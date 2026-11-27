# shader

## add Functional

- [Half-wave scans over helper lanes](../../../core/shader/recompiler/SpirvBackend/src/SpirvAlu/SpirvAluEmitterMath.cpp) (recompiler) - in a wave64 pixel program on a 32-lane host with quad operations, a half-wave scan whose keys are selected by a mask that may hold helper lanes (an `s_wqm_b64` of the entry EXEC, ANDs and phis of such masks, or ballots of predicates built from them) but no lane the wave does not launch builds: `v_readlane_b32` of lane 63 is the identity and lane 31 combines each quad's keys through quad broadcasts before the subgroup reduction, because Vulkan may leave helper invocations out of a subgroup reduction. An add, whose quad combine would count keys more than once, and a mask that may hold unlaunched lanes still throw.
