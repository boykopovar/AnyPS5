# libSceAgc

## change [sceAgcInit]

- [sceAgcInit](../../../core/libs/prx/libSceAgc/DcbState/src/Init.cpp) (libSceAgc) - scalar versions 0–13 still return 0 after selecting register-default tables; values >=14 return 0 without a lookup. Firmware 13.60 [console probe](https://github.com/boykopovar/AnyPS5/issues/4808#issuecomment-6149448503): pointer-shaped calls return 0 without writes, while sampled scalar versions 2 and 7–16 return 0x8A6C0004 there and still return 0 here. Pointer-shaped calls do not establish a register-default table selection. The sceAgcInit_0090 wrapper ignores its first argument and delegates to sceAgcInit; this wrapper behavior follows PPSA06652's bundled module rather than a new console measurement or authenticated retail firmware.
