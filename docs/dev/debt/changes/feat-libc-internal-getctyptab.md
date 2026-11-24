# libSceLibcInternal

## add Unknown function info

- [_Getctyptab](../../../core/libs/prx/libSceLibcInternal/Ctype.cpp) (libSceLibcInternal) - the console module was not read. The `const short*()` prototype and C-locale table follow Dinkumware `_Getctyptab`, forwarding to `libc`'s `_Getpctype` (a 257-entry `short` classification table indexed from `-1` for `EOF` through `255` with the existing Dinkumware masks). Locale switching via `setlocale` does not alter the returned table. Closes when: compared with the console module.
