# libcurl

## change [Guest long]

- [Guest long](../../../core/libs/prx/libcurl/Export.cpp) - LONG easy/multi values outside the host long range throw `std::out_of_range` on LLP64 hosts. Chunk callback long return values and `CURLOPT_HTTPPOST` node lengths and flags have the same restriction.

## add Functional

- [`CURLOPT_HTTPPOST`](../../../core/libs/prx/libcurl/Export.cpp) - the guest node layout is the unverified LP64 mirror of curl 7.81.0's `struct curl_httppost`. The option copies the guest chain into host nodes that borrow every guest pointer and hands those to curl; the easy handle and its `curl_easy_duphandle` copies share the nodes until a reset, cleanup or new `CURLOPT_HTTPPOST`. Curl 7.81.0 reads the caller's nodes again on each perform, so nodes appended or changed after the option is set are not seen until it is set again.
