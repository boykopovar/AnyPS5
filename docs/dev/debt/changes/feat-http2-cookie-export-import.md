# libSceHttp2

## change [libSceHttp2 pending exports]

- [libSceHttp2 pending exports](../../../core/libs/prx/libSceHttp2/Export.cpp) - 2 named callable exports (`sceHttp2SetCookieRecvCallback` and `sceHttp2SetCookieSendCallback`) were identified in the PS5 3.20 export table and cross-checked against firmware 13.60. Signatures and behavior remain unverified; declarations use the unknown-signature convention and throw through `NotImplemented_nid_no_patch`. Evidence: [3.20 export list](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceHttp2.c).

## add Functional

- [sceHttp2CookieExport and sceHttp2CookieImport](../../../core/libs/prx/libSceHttp2/Cookies.cpp) (libSceHttp2) - signatures mirror `sceHttpCookieExport`/`sceHttpCookieImport` on a cookie box ID and are unverified on a console. Export writes a host-defined binary blob (`APS5H2C\x01`, little-endian length-prefixed fields, insertion order, value/secure flags and optional expiry) and import replaces the box with it; console format and replacement-or-merge behavior are unmeasured, so console blobs fail with `ERROR_INVALID_VALUE` (`0x817B11FE`). Null buffer queries required size; short buffer returns `ERROR_OUT_OF_SIZE` (`0x817B1104`); malformed, truncated, oversized (>64 MiB) or unreadable input leaves the box unchanged; expired cookies are dropped, expiry above `INT64_MAX` is clamped, and limit violations return `ERROR_OUT_OF_SIZE`. Unmapped guest pointers return `ERROR_INVALID_VALUE` with `errno` preserved.
