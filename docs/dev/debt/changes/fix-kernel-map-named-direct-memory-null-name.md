# libkernel

## add Functional

- [sceKernelMapNamedDirectMemory](../../../core/libs/prx/libkernel/DirectMemory/Export.cpp) (libkernel) - a name of 32 or more bytes returns `ENAMETOOLONG` and a NULL name returns `EFAULT`; the memory stays mapped and the range keeps no name, which reads as empty here where the console reports `anon:` and hex digits. A PS5 payload process (firmware 11.60) measured 31 characters (named), 32 characters and NULL on 2026-11-07; a title process was not measured, longer names follow from the 32-character case, and an empty name is not measured and is still stored as an empty name. `sceKernelMapNamedFlexibleMemory` was not measured and still cuts a longer name to 31 characters.
