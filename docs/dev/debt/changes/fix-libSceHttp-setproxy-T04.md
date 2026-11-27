# libSceHttp

## change [sceHttpSetProxy]

- [sceHttpSetProxy](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp) - stores the proxy host, port and the HTTP and WLAN proxy-configuration values for a live template, connection or request and returns 0, but no request reaches the network so the stored proxy is never used. The signature `(id, HTTP proxy conf, WLAN proxy conf, host, port)` and the order of the checks (`ERROR_BEFORE_INIT`, then `ERROR_INVALID_VALUE` for a null host, then `ERROR_INVALID_ID`) follow [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/a808e76/src/core/libraries/network/http.cpp) (PS4), which stores the proxy; not measured on a PS5. The host is copied in page-bounded chunks with `GuestCopyIn_nid_no_patch`, and an unreadable host returns `ERROR_INVALID_VALUE` as a host-safety fallback
