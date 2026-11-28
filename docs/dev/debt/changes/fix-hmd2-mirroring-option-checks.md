# libSceHmd2

## add Functional

- [Mirroring option checks](../../../core/libs/prx/libSceHmd2/Export.cpp) (libSceHmd2) - `sceHmd2ReprojectionSetMirroringOption` reads its 32-byte option first: a null or unreadable pointer answers `PARAMETER_NULL` (0x8A720008), a mode other than 0 or 1 or any nonzero reserved word answers `PARAMETER_INVALID` (0x8A720009), and only a valid option reaches the reprojection pipeline, which still throws because it has no host model. Taken from the 13.40 module, which compares the mode and each of the seven reserved words against zero; console execution is not measured.
