# libSceVrHand technical debt

### Silent stubs

- [sceVrHandInitialize, sceVrHandFinalize, sceVrHandStart, sceVrHandStop](../../../core/libs/prx/libSceVrHand/Export.cpp) track lifecycle state transitions without interacting with VR hardware or backend tracking services.

### Unknown function info

- [sceVrHandGetMesh, sceVrHandGetMeshSize, sceVrHandGetResult](../../../core/libs/prx/libSceVrHand/Export.cpp) (libSceVrHand) - names and NIDs verified from pinned ps5rs stubs. They enforce initialization and started state guards returning 0x8a870001 or 0x8a870003; remaining mesh and result query parameters and host runtime behavior remain unverified and throw through NotImplemented_nid_no_patch.
- [sceVrHandQueryMemory](../../../core/libs/prx/libSceVrHand/Export.cpp) (libSceVrHand) - the meaning of the parameter and output fields is unknown. It follows firmware 13.42's libSceVrHand.sprx, read from its disassembly and run natively on x86-64 (not a console): a null pointer, an unreadable parameter, a first 32-bit parameter word other than 0x20, or any nonzero parameter byte from offset 4 to 31 returns `0x8a870006` and writes nothing; an unwritable result pointer returns `0x8a870006` (with possible partial output on copy-out failure). Otherwise it stores the 32-bit values 0x3c00000 and 0x10000 at output offsets 0 and 4, clears output bytes 8 to 27 and returns 0. Returning `0x8a870006` for unwritable or unreadable pointers is a defensive host-protection fallback; console behavior is unmeasured.
