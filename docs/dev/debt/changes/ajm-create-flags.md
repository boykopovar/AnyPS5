# libSceAjm.native

## add Unknown function info

- [sceAjmInstanceCreate flags](../../../core/libs/prx/libSceAjm.native/src/Ajm.cpp) (libSceAjm.native) - firmware 13.42's library checks a null instance output before rejecting `(flags & 0x380) >= 0x180`, `(flags & 0x2400) == 0x2000`, or any bit in `0xf0000000ffffc000` with `0x8093000c`; only then does it make the AJM device request. This implementation applies those pre-driver checks. The driver's result for a valid request and the exact error precedence of its context validation remain unmeasured here. The reading is disassembly only, not a console run ([#21856](https://github.com/boykopovar/AnyPS5/issues/21856#issuecomment-6939955028)).
