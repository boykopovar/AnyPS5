# libcurl

## add Functional

- [curl_strequal and curl_strnequal](../../../core/libs/prx/libcurl/Export.cpp) (libcurl) - both read their guest strings through `GuestCopyIn_nid_no_patch`, never past the page of the byte being compared; an unreadable string throws, as a null one does, because what the console module does on unmapped memory is unmeasured. `curl_strnequal` no longer reads a byte once `max` is spent, so a string that ends exactly at `max` bytes before an unmapped page compares equal.
