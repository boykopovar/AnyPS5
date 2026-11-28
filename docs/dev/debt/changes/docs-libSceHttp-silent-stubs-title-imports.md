# libSceHttp

## add Silent stubs

- [sceHttpSendRequest](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp) - returns `ERROR_NETWORK` with `(void)`-cast `request_id`, `post_data` and `size`, so no request reaches the network. PPSA85300 (VLC) and PPSA99666 (DOOM) import it (NID `1e2BNwI-XzE`, one reference each), and the console's `libSceHttp` exports that NID.
- [sceHttpGetStatusCode](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp) - returns `ERROR_NETWORK` and leaves `status_code` untouched, as no response is received. PPSA85300 (VLC) and PPSA99666 (DOOM) import it (NID `0a2TBNfE3BU`, one reference each), and the console's `libSceHttp` exports that NID.
- [sceHttpReadData](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp) - returns `ERROR_NETWORK` and writes nothing to `data`, as no response body exists to read. PPSA85300 (VLC) and PPSA99666 (DOOM) import it (NID `P5pdoykPYTk`, one reference each), and the console's `libSceHttp` exports that NID.
- [sceHttpGetAllResponseHeaders](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp) - returns `ERROR_NETWORK` and leaves `header` and `header_size` untouched, as no response headers exist. PPSA85300 (VLC) and PPSA99666 (DOOM) import it (NID `aCYPMSUIaP8`, one reference each), and the console's `libSceHttp` exports that NID.
- [sceHttpGetResponseContentLength](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp) - returns `ERROR_NETWORK` and leaves `result` and `content_length` untouched, as no response exists to measure. PPSA85300 (VLC) and PPSA99666 (DOOM) import it (NID `yuO2H2Uvnos`, one reference each), and the console's `libSceHttp` exports that NID.
