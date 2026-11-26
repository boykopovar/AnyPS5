# libSceVideoOut

## add Silent stubs

- [sceVideoOutConfigureOutputMode_](../../../core/libs/prx/libSceVideoOut/InventoryExports.cpp) (libSceVideoOut) - a mode record left at "any" in every field, on the main bus with a null fourth argument, returns 0 without the request firmware 13.42's module hands to libSceAvSetting; the output stays as it is (see its Unknown function info entry)

## change [sceVideoOutGetDeviceInfoEx_, sceVideoOutConfigureOutputMode_]

- [sceVideoOutGetDeviceInfoEx_, sceVideoOutConfigureOutputMode_](../../../core/libs/prx/libSceVideoOut/InventoryExports.cpp) (libSceVideoOut) - check their arguments in the order firmware 13.42's `libSceVideoOut.sprx` does ([disassembly](https://github.com/boykopovar/AnyPS5/issues/15134#issuecomment-6755215803), not run on a console). `GetDeviceInfoEx_` then throws: it fills its 0x18 bytes from an import with no known name (`0BKuKlQG8zE`). `ConfigureOutputMode_` then throws for a record whose first word is 0x20, except the one left at "any" below. The handle lookup's 0x8029001b for a record whose `+0x10` is non-zero is never returned: that state is not modelled. `ConfigureOutputMode_` also throws on a port off the main bus, since how the open's bus maps to the device's (0 or 0x200 accepted) is unknown, and for the mode a module global decides (`+0x10` 0xffffffff21ffffff, `+8` 13, 16 or 18). What its size argument of 0x10 sizes is unknown.

## add Unknown function info

- [sceVideoOutConfigureOutputMode_ with a record left at "any"](../../../core/libs/prx/libSceVideoOut/InventoryExports.cpp) (libSceVideoOut) - the console's result for a record whose first word is 0x20 and whose bytes 4 to 0x1f are all 0xff (as `sceVideoOutModeSetAny_` fills it), with a null fourth argument, is not measured; it returns 0 here. In a [validator run](https://github.com/boykopovar/AnyPS5/issues/15134#issuecomment-6907373767) on a PC harness that record passed with the flags word clear (no result with flags set is reported); the module then checks the output's owner, which cannot fail here as each bus has one handle, and returns libSceAvSetting's reply, which was not read. That reading is of the PS5 module. Closes when: measured on a console.
