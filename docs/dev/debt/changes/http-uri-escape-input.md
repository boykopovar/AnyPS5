# libSceHttp

## add Functional

- [sceHttpUriEscape input access](../../../core/libs/prx/libSceHttp/src/Uri.cpp) (libSceHttp) - returning `SCE_HTTP_ERROR_INVALID_VALUE` (`0x804311FE`) for an unreadable input byte through `GuestCopyIn_nid_no_patch`, preserving host errno and leaving both outputs untouched, is a host-safety fallback; console behavior on inaccessible or unterminated input is unmeasured.
