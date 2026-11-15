# libSceAgc

## change [sceAgcInit]

- [sceAgcInit](../../../core/libs/prx/libSceAgc/DcbState/src/Init.cpp) (libSceAgc) - platform registration, module-origin selection and hardware initialization callbacks are not reproduced. The two cached context-default instances follow PPSA06652's bundled module: versions 0–13 reuse an existing instance or occupy an empty one; a third distinct version returns 0x8A6C0004. Values >=14 still return 0 without a lookup, unlike sampled scalar calls in the firmware 13.60 [console probe](https://github.com/boykopovar/AnyPS5/issues/4808#issuecomment-6149448503). Pointer-shaped calls do not establish a register-default table selection. The sceAgcInit_0090 wrapper's ignored first argument follows the bundled module, not authenticated retail firmware or a new console measurement.
