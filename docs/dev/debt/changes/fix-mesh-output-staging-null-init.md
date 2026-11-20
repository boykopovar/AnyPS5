# shader

## add Functional

- [Mesh output staging](../../../core/shader/recompiler/SpirvBackend/src/SpirvModuleEmitter.cpp) (shader recompiler) - a mesh shader declares the Private arrays that hold each lane's exports until the outputs are written (position, parameters, clip and cull distances, point size, the primitive word) with an `OpConstantNull` initializer. Without it, NVIDIA's Windows driver (610.88, RTX 3050) gave every vertex the `w` of a position export that only runs for a group without primitives: SwanStationPS5 (PPSA98510) 1.6.3 and later, whose NGG culling program exports one NaN primitive for an empty group (Mesa's GFX10.1 workaround), drew nothing, while AMD's Windows driver drew the same SPIR-V. A lane that never exports now reads 0 instead of an undefined value.
