# libSceDbgThreadSanitizer technical debt

### Unknown function info

- [__tsan_acquire, __tsan_release](../../../core/libs/prx/libSceDbgThreadSanitizer/Export.cpp) (libSceDbgThreadSanitizer, NIDs `XyX1104ACcU` and `IkoI44e0OXc`) - are fast no-ops because AnyPS5 runs no race detector. The `void (void* addr)` signatures are those of LLVM compiler-rt's [tsan_interface.h](https://github.com/llvm/llvm-project/blob/87f0227cb60147a26a1eeb4fb06e3b505e9c7261/compiler-rt/include/sanitizer/tsan_interface.h#L24-L25), whose names hash to the imported NIDs; that the console library has the same signatures is assumed. PPSA23226's bundled `libSceJobManager_nosubmission.prx` imports both and calls them only after [sceKernelIsThreadSanitizerEnabled](../../../core/libs/prx/libkernel/System/src/Sanitizer.cpp) reports the sanitizer enabled, which it never does here
