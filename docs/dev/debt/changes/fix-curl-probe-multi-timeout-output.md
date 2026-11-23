# libcurl

## add Functional

- [curl_multi_timeout](../../../core/libs/prx/libcurl/Export.cpp) (libcurl) - the host timeout is written to the guest with `GuestCopyOut_nid_no_patch`; returning `CURLM_BAD_FUNCTION_ARGUMENT`, the null-output result, for a non-null unwritable output is a host-safety fallback, and what the console module does on unmapped memory is unmeasured.
