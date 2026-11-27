# libSceWorkspace

## change [module_start]
- [module_start](../../../core/libs/prx/libSceWorkspace/Export.cpp) (libSceWorkspace) - unknown signature (NID BaOKcng8g88). A PS5 (firmware 11.60, payload process; a title process may differ) did not load the module: `dlopen` of libSceWorkspace.sprx returned NULL in three attempts in two runs; in the last run `dlerror` was read after each attempt and reported only the code `0xffffffff`, so nothing was measured about the names of the module.
