# libSceHttp2

## add Functional

- [sceHttp2ReadData and sceHttp2SendRequest](../../../core/libs/prx/libSceHttp2/Transport.cpp) (libSceHttp2) - `sceHttp2ReadData` copies the response out with `GuestCopyOut_nid_no_patch` (HTTP/1 through a 16 KiB host staging buffer, buffered bodies and gzip directly) and `sceHttp2SendRequest` copies `post_data` in once with `GuestCopyIn_nid_no_patch` before connecting; an inaccessible non-null `data` or `post_data` returns `0x817B11FE` (`ERROR_INVALID_VALUE`) while `errno` is preserved, as the cookie and WebSocket receive buffers in this library do, because the console's behaviour on unmapped memory is unmeasured.
