# libSceNpWebApi

## remove sceNpWebApiGetErrorCode

## add Unknown function info

- [sceNpWebApiGetErrorCode](../../../core/libs/prx/libSceNpWebApi/Export.cpp) (libSceNpWebApi) - firmware 13.42 forwards to `sceNpServerErrorJsonGetErrorCode` below SDK 2.50 and its `Multi` form from SDK 2.50, then maps two parser errors. The firmware module's version global was not traced to its source; this uses the title's compiled SDK version as the rest of this library does. Read from disassembly, not measured on a console.
