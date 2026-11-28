# libSceLibcInternal

## add Unknown function info

- [_Deletegloballocale](../../../core/libs/prx/libSceLibcInternal/LocaleObjects.cpp) (libSceLibcInternal) - the console module was not read. The `void(void*)` prototype and the behaviour follow the Dinkumware `_Deletegloballocale` of the first MSVC STL release ([locale0.cpp](https://github.com/microsoft/STL/blob/219514876ea86491de191ceaa88632616bbc0f19/stl/src/locale0.cpp)): the argument points to a `_Locimp` reference, a null reference is ignored, otherwise the locale loses one reference through its `_Decref` slot and is deleted through its deleting destructor when that was the last one. The reference itself is not cleared. A null argument throws instead of being dereferenced. Closes when: compared with the console module.
