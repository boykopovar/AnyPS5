# libSceVdecSavc2

## add Functional

- [sceSdecCreateSw2 and sceSdecDestroySw2 handle lifecycle](../../../core/libs/prx/libSceVdecSavc2/Export.cpp) - Updated to follow firmware 13.60 handle and workspace lifecycle (`sceSdecCreateSw2` initializes the 24-byte handle header, 32-byte workspace allocator header, codec state at `+0x10`, and zeroes `blocks[1]`, returning 0 when all guest writes succeed and `unknown20 != 0`, or clearing initialized workspace/codec/handle state and returning `SdecInvalid` on write failure; `sceSdecDestroySw2` clears `+0x08` and `+0x10` and returns 0 when writable). Thread/mutex creation and codec virtual teardown remain unemulated.
