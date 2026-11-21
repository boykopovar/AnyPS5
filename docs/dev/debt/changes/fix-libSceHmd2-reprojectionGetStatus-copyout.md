# libSceHmd2

## add Unknown function info

- [sceHmd2ReprojectionGetStatus](../../../core/libs/prx/libSceHmd2/Export.cpp) (libSceHmd2) - returning `ParameterNull` for an unwritable reprojection status output via `CopyGuestOut` with caller errno preserved is a host-safety fallback with console behavior unmeasured.
