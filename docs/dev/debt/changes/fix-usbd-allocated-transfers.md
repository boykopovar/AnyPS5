# libSceUsbd

## add Functional

- [sceUsbdFillInterruptTransfer, sceUsbdFillBulkTransfer, sceUsbdFillIsoTransfer, sceUsbdFillControlTransfer, sceUsbdControlTransferGetData, sceUsbdControlTransferGetSetup, sceUsbdSetIsoPacketLengths, sceUsbdGetIsoPacketBuffer](../../../core/libs/prx/libSceUsbd/Export.cpp) (libSceUsbd) - host memory-safety check, not measured on a console: a transfer that `sceUsbdAllocTransfer` did not return, or that `sceUsbdFreeTransfer` already freed, throws before any of its fields is read or written, as an unknown device or handle does. Transfers are host allocations here, so such a pointer would otherwise be written as host memory. FreeBSD 11.4 [libusb10.c](https://github.com/freebsd/freebsd-src/blob/d106002967eff712894ab8b5337bbbff106ce5e0/lib/libusb/libusb10.c) keeps its private transfer state in front of each transfer `libusb_alloc_transfer` returns; what the console's fill calls do with another pointer is not known. Closes when: measured on a PS5
