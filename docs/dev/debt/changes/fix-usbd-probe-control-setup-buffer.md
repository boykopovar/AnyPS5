# libSceUsbd

## add Functional

- [sceUsbdFillControlSetup and sceUsbdFillControlTransfer setup buffer](../../../core/libs/prx/libSceUsbd/Export.cpp) (libSceUsbd) - the eight setup bytes are written, and the setup's data length is read before the transfer is filled, with checked guest copies; an unwritable or unreadable setup buffer throws, as a null one to `sceUsbdFillControlSetup` already does, because whether the console faults on unmapped memory is unmeasured.
