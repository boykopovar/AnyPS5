# libSceNpWebApi

## remove sceNpWebApiGetErrorCode

## add Unknown function info

- [sceNpWebApiGetErrorCode](../../../core/libs/prx/libSceNpWebApi/Export.cpp) (libSceNpWebApi) - firmware 13.42 `libSceNpWebApi.sprx` disassembly selects `sceNpServerErrorJsonGetErrorCode` below SDK 2.50 and its `Multi` form from SDK 2.50, then maps `0x80559c02` and `0x80559c03`. The module's version global was not traced to its source; this uses the title's compiled SDK version as the rest of this library does. The current libSceNpCommon parser converts parse failures to HTTP-status errors before either mapped code can reach this export. Nothing was measured on a console.
