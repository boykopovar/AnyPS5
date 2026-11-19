# libSceHttp

## add Unknown function info

- [sceHttpAddQuery](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp) - returns 0 for any id and keeps nothing: the query string is ignored, so a request's query is silently dropped. No request reaches the network. [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/main/src/core/libraries/network/http.cpp) likewise only logs and returns 0. Not measured on a console.
