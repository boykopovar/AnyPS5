# libSceJsc

## add Unknown function info

- [JSStringGetUTF8CString_nid_postfix](../../../core/libs/prx/libSceJsc/Export.cpp) (libSceJsc) - UTF-8 bytes are staged in fixed 4 KiB chunks, respecting complete code points and the caller's byte capacity, then copied through `GuestCopyOut_nid_no_patch` with `errno` preserved. A failed write or guest-address overflow returns zero instead of crashing the host. Successful conversions retain the original null terminator, UTF-8 truncation and byte-count behavior. The error result for an unmapped non-null guest buffer is a host-safety fallback; retail console behavior has not been measured.
