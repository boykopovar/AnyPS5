# libSceVideoOut

## remove | `sceVideoOutSetAlphaControl` | `-zRnJ7WoWSY` |

## remove | `sceVideoOutSetGlobalAlpha` | `Ujsmw0VOqOo` |

## add Unknown function info

- [sceVideoOutSetGlobalAlpha, sceVideoOutSetAlphaControl](../../../core/libs/prx/libSceVideoOut/InventoryExports.cpp) (libSceVideoOut) - take `(handle, enable, alpha)` and `(handle, enable, alpha, control, reserved)` and check them as firmware 13.42's `libSceVideoOut.sprx` does ([disassembly](https://github.com/boykopovar/AnyPS5/issues/15134#issuecomment-6938654570), not run on a console): `SetAlphaControl` refuses a non-null fifth argument with 0x8029001a first, then a handle that is not open gives 0x8029000b. On the main bus both return 0x8029001b, as the module refuses bus types 0, 0x200 and 0x210 before it reads `enable` or `alpha`. A port off the main bus throws, since its device bus type and the handle record's `+0x10` aren't modelled; past them the call checks `enable` (0x8029001e) and `alpha` (0x80290001) and reaches the video driver.
