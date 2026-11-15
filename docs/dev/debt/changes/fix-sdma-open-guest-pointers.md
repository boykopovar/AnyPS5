# libSceSdma

## add Functional

- [sceSdmaOpen guest pointers](../../../core/libs/prx/libSceSdma/Export.cpp) - `param` and `handle` are accessed through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch`, returning `SdmaErrorInvalidArgument` (`0x80990005`) with caller `errno` preserved and no slot left open if either pointer is unreadable or unwritable; console behaviour on unmapped pointers is unmeasured.
