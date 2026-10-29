# libSceVrHand technical debt

### Unknown function info

- [sceVrHandFinalize, sceVrHandGetMesh, sceVrHandGetMeshSize, sceVrHandGetResult, sceVrHandInitialize, sceVrHandQueryMemory, sceVrHandStart, sceVrHandStop](../../../core/libs/prx/libSceVrHand/Export.cpp) (libSceVrHand) - names and NIDs verified from pinned [ps5rs stubs](https://github.com/claimore22/ps5rs/tree/5f252a658ff470bcb2b6cd15205fd8ca64ae4477/data/stubs/by_library) (`libSceVrHand.txt`), with NID match confirmed via `tools/nid_names.py` `compute_nid`. Signatures, parameter layouts, and runtime behavior remain unverified; all paths throw through `NotImplemented_nid_no_patch`.
