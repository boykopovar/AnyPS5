# libSceVdecCore

## add Functional

- [sceVdecCoreGetDecodeOutput](../../../core/libs/prx/libSceVdecCore/Export.cpp) (libSceVdecCore) - copying frame buffer inputs and output info via `CopyGuestIn` and `CopyGuestOut`, returning `VdecCoreErrorInvalidArgument` while preserving `errno` for unmapped or unwritable guest pointers, is a host-safety fallback; console fault behavior on invalid buffers has not been measured.
