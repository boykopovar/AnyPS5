# libSceDiscMap

## add Functional

- [libSceDiscMapUnknown_5_plus_vOlukvkfg_nid_no_patch](../../../core/libs/prx/libSceDiscMap/Export.cpp) (libSceDiscMap) - returns `SCE_DISC_MAP_ERROR_NO_BITMAP_INFO` (`0x81100004`) before reading arguments when no disc bitmap info is loaded, matching firmware 13.60 `libSceDiscMap.sprx` disassembly at `0x12d0` (not measured on a console); the bitmap-loaded path remains unimplemented.
