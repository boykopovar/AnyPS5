# libSceVdecSavc2

## add Functional

- [sceSdecCreateSw2 and sceSdecDestroySw2 handle lifecycle](../../../core/libs/prx/libSceVdecSavc2/Export.cpp) - Updated to follow firmware 13.60 handle and workspace lifecycle (`sceSdecCreateSw2` pre-checks writability of the handle header, workspace header, codec object, output `*handle`, and non-zero `blocks[1]` before writing any byte, then initializes the 24-byte handle header, 32-byte workspace allocator header, codec state at `+0x10`, and zeroes `blocks[1]`, returning 0 when all guest writes succeed and `unknown20 != 0`, or rolling back any partially written regions and returning `SdecInvalid` on failure; `sceSdecDestroySw2` clears `+0x08` and `+0x10` and returns 0 when writable). Thread/mutex creation and codec virtual teardown remain unemulated.
