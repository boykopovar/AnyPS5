# libSceVrHand

## change sceVrHandFinalize

- [sceVrHandGetMesh and sceVrHandGetResult](../../../core/libs/prx/libSceVrHand/Export.cpp) (libSceVrHand) - uninitialized checks validate module state returning `0x8a870001` before delegating to unimplemented service body via `NotImplemented_nid_no_patch(__func__)`. Output layout and console behavior remain unmeasured.

## add Functional

- [sceVrHandFinalize, sceVrHandInitialize, sceVrHandStart and sceVrHandStop](../../../core/libs/prx/libSceVrHand/Export.cpp) (libSceVrHand) - lifecycle state transitions, parameter record validation, and start/stop controls are implemented according to firmware 13.42 disassembly (`libSceVrHand.sprx` sha256 `8d211358c9e9a632d7289d7a5dcbb73ec3193a734cea20caa7ee4111d7295dcc`).
