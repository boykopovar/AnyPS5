# libSceLibcInternal

## add Unknown function info

- [_Getgloballocale and _Setgloballocale](../../../core/libs/prx/libSceLibcInternal/LocaleObjects.cpp) (libSceLibcInternal) - the console module was not read. The `void*()` and `void(void*)` prototypes and behaviour follow Dinkumware `_Getgloballocale` and `_Setgloballocale` in the first MSVC STL release ([locale0.cpp](https://github.com/microsoft/STL/blob/219514876ea86491de191ceaa88632616bbc0f19/stl/src/locale0.cpp)): an atomic `_Locimp*` starts null, `_Getgloballocale` returns it and `_Setgloballocale` stores the caller's pointer (including null) without modifying reference counts. Unlike upstream, `_Setgloballocale` does not register process-exit `_Atexit(tidy_global)` cleanup, and this storage is independent of `libc`'s `_ZNSt6locale16_GetgloballocaleEv`. Closes when: compared with the console module.
