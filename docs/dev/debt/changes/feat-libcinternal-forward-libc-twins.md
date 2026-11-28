# libSceLibcInternal

## remove Wut42WAe7Rw

## add Functional

- [_new_setup](../../../core/libs/prx/libSceLibcInternal/InventoryExports.cpp) and [_ZNSt10filesystem18_Xfilesystem_errorEPKcRKNS_4pathES4_St10error_code](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal, NIDs `KNNNbyRieqQ` and `Wut42WAe7Rw`) run libc's exports of the same NIDs, so the limits of the libc `_new_setup` and `std::filesystem::_Xfilesystem_error` entries apply and the CRT slots `_new_setup` records are libc's. Neither `libSceLibcInternal.sprx` body was read, so whether it differs from the `libc.prx` one those entries follow is not known, and no title was seen importing either NID from this library
