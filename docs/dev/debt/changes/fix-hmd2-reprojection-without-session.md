# libSceHmd2

## add Functional

- [Reprojection frame and mirroring calls](../../../core/libs/prx/libSceHmd2/Export.cpp) - `sceHmd2ReprojectionBeginFrame`, `sceHmd2ReprojectionDisableMirroring`, `sceHmd2ReprojectionDisableVrMode` and `sceHmd2ReprojectionSetMirroringOption` report `0x8a72000b` without a session, matching the [firmware 13.60 console log without a headset](https://github.com/boykopovar/AnyPS5/pull/7798#issuecomment-6262489180). With a session they still throw; their active-session behavior is not implemented.
