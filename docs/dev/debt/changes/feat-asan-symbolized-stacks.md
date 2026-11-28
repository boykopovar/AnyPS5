# libSceAddressSanitizer

## add Functional

- [ASan stacks, symbolization and coverage](../../../core/libs/prx/libSceAddressSanitizer/src/AsanSymbolizer.cpp) (libSceAddressSanitizer) - frame-pointer unwinding, the `stack_trace_format` frame renderer, report summaries, `__sanitizer_symbolize_pc`, `__sanitizer_symbolize_global` and the [SanitizerCoverage guard exports](../../../core/libs/prx/libSceAddressSanitizer/src/AsanCoverage.cpp) follow compiler-rt 16.0.6. Modules and offsets come from `sceKernelGetModuleInfoFromAddr`, and function names from the host symbol table through `dladdr`, so Windows frames have no names and none have file or line. Guest code built without frame pointers yields short stacks. Threads are named by `scePthreadGetthreadid` and `scePthreadGetname`, and the "Thread T created by" lines are not printed. Coverage is dumped at host `atexit` and on a fatal report.
