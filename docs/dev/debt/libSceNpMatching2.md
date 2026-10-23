# libSceNpMatching2 technical debt

### Functional

- [sceNpMatching2Initialize, sceNpMatching2Terminate](../../../core/libs/prx/libSceNpMatching2/Export.cpp) (libSceNpMatching2) - only the library state is kept: initialization checks the parameter's `size` field (0x28, or 0x30 with an SSL pool) as [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/81be949167d0060b08f26dc3e055a0b640d129c8/src/core/libraries/np/np_matching2/np_matching2.cpp) does and starts no thread; a second initialization returns `ALREADY_INITIALIZED` and a termination without one `NOT_INITIALIZED`. The pool, thread and SSL settings are not used: no matching server is reached. Not measured on a console
