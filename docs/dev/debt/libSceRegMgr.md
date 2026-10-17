# libSceRegMgr technical debt

### Unknown function info

- [sceRegMgrNonSysGetInt](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr, NID `dKeshzt29G4`) - imported by Astro Bot (PPSA21564), which fails to load without libSceRegMgr.prx (issue 4720); the `(int64_t key, int32_t unk, int32_t* data)` signature follows [ChonkyStation4](https://github.com/liuk7071/ChonkyStation4/blob/c101a23ecbc15e56c6e30d5dd24d3ed8880a957a/ChonkyStation4/OS/Libraries/SceRegMgr/SceRegMgr.cpp#L38-L47), not measured on a console, so every call throws
