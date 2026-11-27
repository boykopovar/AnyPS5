# libcurl

## add Functional

- [curl_formfree](../../../core/libs/prx/libcurl/src/FormFree.cpp) reads each `guest_curl_httppost` node through `GuestCopyIn_nid_no_patch` while preserving `errno`, throwing `std::invalid_argument` if a node pointer is unreadable instead of dereferencing guest memory directly.
