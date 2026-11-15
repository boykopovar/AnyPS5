# libSceAgc

## change [sceAgcGetDefaultCxStateFlat]

- [sceAgcGetDefaultCxStateFlat](../../../core/libs/prx/libSceAgc/Misc/src/RegisterDefaults.cpp) (libSceAgc) - the four-argument range reader and sparse public/internal CX backing follow PPSA06652's bundled module, not authenticated retail firmware or a fresh console measurement. Instance indices above 1 throw; invalid nonempty output returns 0x80890001 as host validation instead of reproducing undefined reference faults. Full platform initialization and driver-supplied default adjustments are not reproduced. Trinity selection follows the host kernel's sceKernelIsTrinityMode, currently the base profile; that alternate profile was not executed.

## change [sceAgcInit]

- [sceAgcInit](../../../core/libs/prx/libSceAgc/DcbState/src/Init.cpp) (libSceAgc) - platform registration, module-origin selection and hardware initialization callbacks are not reproduced. The two cached context-default instances follow PPSA06652's bundled module: versions 0–13 reuse an existing instance or occupy an empty one; a third distinct version returns 0x8A6C0004. Values >=14 still return 0 without a lookup, unlike sampled scalar calls in the firmware 13.60 [console probe](https://github.com/boykopovar/AnyPS5/issues/4808#issuecomment-6149448503). Pointer-shaped calls do not establish a register-default table selection. The sceAgcInit_0090 wrapper's ignored first argument follows the bundled module, not authenticated retail firmware or a new console measurement.
