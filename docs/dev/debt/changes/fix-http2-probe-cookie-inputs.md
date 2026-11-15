# libSceHttp2

## add Unknown function info

- [cookie URL and text inputs](../../../core/libs/prx/libSceHttp2/Cookies.cpp) (libSceHttp2) - `sceHttp2AddCookie` and `sceHttp2GetCookie` copy the URL with `CopyGuestString_nid_no_patch` (up to 64 KiB), and `sceHttp2AddCookie` copies its `cookieLength` bytes of cookie text with `Http2Guest::CopyIn`; an unreadable URL or cookie returns `SCE_HTTP2_ERROR_INVALID_VALUE` like a null one, because the console's behaviour on unmapped memory is unmeasured.
