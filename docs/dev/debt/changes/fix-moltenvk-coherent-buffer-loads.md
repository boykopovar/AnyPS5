# shader

## add Functional

- [GLC and DLC buffer accesses on MoltenVK](../../../core/shader/recompiler/SpirvBackend/src/SpirvRuntimeBuffer.cpp) (shader recompiler) - on MoltenVK (`SpirvTarget::coherentBufferAtomics`) a GLC or DLC dword load through a bound buffer descriptor is a relaxed device-scope `OpAtomicLoad`, an aligned dword store an `OpAtomicStore`, a misaligned or sub-dword store a compare-and-swap per dword; a dwordx2 is two such dwords, so it can tear where the console's cannot. `s_waitcnt_vscnt` and `buffer_gl0_inv`/`buffer_gl1_inv` are device-scope acquire-release fences. Other drivers keep `Volatile` accesses and no fence, unless `APS5_COHERENT_ATOMIC_LOADS` (see Coherent buffer loads) makes GLC loads atomic. Not covered: the BDA path (FLAT and global), where GLC stores and sub-dword or misaligned loads stay `Volatile`; GLC scalar loads; a plain store racing a GLC load. Guest ordering without those instructions is the host GPU's. Measured only on an Apple M5 Max.
