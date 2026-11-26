# shader

## add Functional

- [Mesh barycentric emulation outputs](../../../core/shader/recompiler/SpirvBackend/src/SpirvModuleEmitter.cpp) (shader recompiler) - Mesa ANV 26.2.7 on an Arc B390 reads the per-primitive inputs of a fast-linked graphics pipeline library one location late when the mesh shader writes an odd number of per-primitive locations that the fragment shader does not read. Monolithic pipelines, link-time-optimized libraries and llvmpipe read them at their locations. The emulation writes the corner positions only when the pixel shader reads barycentrics, so it writes only per-primitive locations the pixel shader declares. A declared per-vertex parameter whose reads the driver removes as dead would still move the inputs after it. Closes when: ANV reads per-primitive inputs at their locations in fast-linked libraries.
