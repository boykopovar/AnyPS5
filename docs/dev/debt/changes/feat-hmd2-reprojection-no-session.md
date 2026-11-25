# libSceHmd2

## add Functional

- [sceHmd2ReprojectionBeginFrame, sceHmd2ReprojectionDisableMirroring, sceHmd2ReprojectionDisableVrMode and sceHmd2ReprojectionSetMirroringOption](../../../core/libs/prx/libSceHmd2/Export.cpp) (libSceHmd2) - inside a reprojection session they throw: their answers there are not measured. Without a session each returns 0x8a72000b (`REPROJECTION_NOT_INITIALIZED`) for every argument and writes nothing, as a retail PS5 answered all four from a payload before `sceHmd2Initialize` with null and zeroed arguments ([firmware 13.60](https://github.com/boykopovar/AnyPS5/pull/7798#issuecomment-6262489180)) and `BeginFrame` and `SetMirroringOption` with non-zero arguments in a game process whose `sceHmd2Initialize` and `sceHmd2ReprojectionInitialize` had failed ([13.40](https://github.com/boykopovar/AnyPS5/pull/8645#issuecomment-6307024384))
