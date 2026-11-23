# libkernel

## change [sceKernelPrintBacktraceWithModuleInfo](../../../core/libs/prx/libkernel/Unimplemented.cpp) (libkernel)

- [sceKernelPrintBacktraceWithModuleInfo](../../../core/libs/prx/libkernel/Unimplemented.cpp) (libkernel) - imported by PS4 titles, which do not load without it, and added without an implementation: declared without parameters, every call throws. A PS5 Pro payload process (13.60) showed the contract in three calls across three sessions, each returning 0: it prints a `backtrace:` header line and one line per caller-stack frame, `[d]    0x<address> => (<symbol>|NoName)<module>+0x<offset>`, attributing every frame to its module - unresolved symbols print `NoName` - and ends with a frame line whose address is zero. What the output stream is on the system console was not distinguished from stdout here; a title process was not measured
