# libc

## add Functional

- [libc++ string-taking runtime_error constructors](../../../core/libs/prx/libc/src/exception/LibcxxRuntimeErrorString.cpp) (libc) supply the complete-object and base-object imports called by RetroArch Azahar (PPSA99169, #5927). They read the observed libc++ string layout and pass the C-string diagnostic to the existing owned runtime_error storage. The firmware provider and full title behavior remain unmeasured.
