# libSceJxr

## add Functional

- [GetPosWS_File and GetPosWS_List](../../../core/libs/prx/libSceJxr/src/FileStream.cpp) write the position through `GuestCopyOut_nid_no_patch` and throw the same `std::invalid_argument` for an unwritable output as for a null one, as `GetPosWS_Memory` already rejects both; whether the console faults or returns an error on unmapped memory is unmeasured.
