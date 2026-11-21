# libSceOpusCeltDec

## change [sceOpusCeltDecCreate]

- [sceOpusCeltDecCreate](../../../core/libs/prx/libSceOpusCeltDec/Export.cpp) (libSceOpusCeltDec) - `(void* state, int sampleRate, int channels)` is `sceOpusCeltDecCreateEx` without the context argument, as firmware 13.42's module shows ([reading](https://github.com/boykopovar/AnyPS5/issues/21851#issuecomment-6939881723), disassembly only, not run on a console): the same checks in the same order (a null `state`, then channels before the rate) and the same worker, with the AJM context taken from a module-wide one the module creates at load. Here that context is created on the first call, so no `sceOpusCeltDecInitialize` is needed, as on the console. What the console returns when its load-time context creation failed wasn't read.
