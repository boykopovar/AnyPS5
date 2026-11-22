# libcurl

## remove [`curl_formget`](../../../core/libs/prx/libcurl/Export.cpp) - throws through `NotImplemented_nid_no_patch`: LP64

## add Functional

- [`curl_formget`](../../../core/libs/prx/libcurl/Export.cpp) - parts marked `CURL_HTTPPOST_FILENAME` or `READFILE` throw through `NotImplemented_nid_no_patch`, as curl would open the path, or stdin for `-`, on the host. On LLP64 hosts a length that does not fit a host `long` throws `std::out_of_range`, like the Guest long entry above. The guest node layout is the unverified LP64 mirror of curl 7.81.0's `struct curl_httppost`. Other nodes are copied into host nodes, including shared and cyclic `next`/`more` links, and passed to curl 7.81.0's [curl_formget](https://github.com/curl/curl/blob/801bd5138ce31aa0d906fa4e2eabfc599d74e793/lib/formdata.c#L712-L740), whose 8192-byte chunks reach the guest append callback.
