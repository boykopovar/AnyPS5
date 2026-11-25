# libSceFiber

## change [sceFiberOptParamInitialize]

- [sceFiberOptParamInitialize](../../../core/libs/prx/libSceFiber/Export.cpp) returns SCE_FIBER_ERROR_INVALID when the destination pointer is unwritable. This is a defensive host-protection fallback: on a PS5 (FW 11.60) a block at 0x1000 or on a read-only page faults with SIGSEGV and none of its bytes are written, and that fault is not reproduced here. Whether 0x1000 is unmapped is not measured. A block that is not 8-byte aligned returns SCE_FIBER_ERROR_ALIGNMENT; the console failed +1, +2 and +4 and accepted +8, +16 and +32, and +3 and +5 to +7 are not measured, so this library applies the same 8-byte rule to them.
