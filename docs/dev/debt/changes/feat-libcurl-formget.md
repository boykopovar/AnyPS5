# libcurl

## remove [`curl_formget`]

## add Functional

- [`curl_formget`](../../../core/libs/prx/libcurl/Export.cpp) - the guest node layout is the unverified LP64 mirror of curl 7.81.0's `struct curl_httppost`. The export copies each guest node through `GuestCopyIn_nid_no_patch` into a host node that borrows its pointers and serializes them with the bundled curl, so the multipart boundary is random as in curl. An unreadable node throws through `NotImplementedAt_nid_no_patch`. On LLP64 hosts a length or flags value that does not fit a host `long` throws `std::out_of_range`. `CURLFORM_FILE` and `CURLFORM_FILECONTENT` names reach host `fopen` untranslated, as with `CURLOPT_HTTPPOST`.
