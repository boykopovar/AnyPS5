# libSceHttp

## add Silent stubs

- [sceHttpRequestGetAllHeaders](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp) - returns 0 headers and an empty content length for a live request, since `sceHttpAddRequestHeader` keeps nothing and no request reaches the network. An unknown id returns `ERROR_INVALID_ID` even before init, a known id before init `ERROR_BEFORE_INIT`, a null `headerCount` `ERROR_INVALID_VALUE`; a null `contentLength` with a size pointer reports the required size of 1, and a buffer smaller than that returns `ERROR_OUT_OF_SIZE`. Not measured on a console.
