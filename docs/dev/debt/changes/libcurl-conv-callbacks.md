# libcurl

## change [Native callback objects]

- [Native callback objects](../../../core/libs/prx/libcurl/Export.cpp) - `CURLOPT_SSL_CTX_FUNCTION` and `SSL_CTX_DATA` throw `std::invalid_argument` naming the option because mbedtls context objects belong to the host. The three `CURLOPT_CONV_*_FUNCTION` options are stored by native curl and return `CURLE_OK`, as pinned [curl 7.81.0 setopt.c](https://github.com/curl/curl/blob/801bd5138ce31aa0d906fa4e2eabfc599d74e793/lib/setopt.c#L1653-L1670) does; this build has no `CURL_DOES_CONVERSIONS`, so curl never calls them. TLS session/SSL pointer getinfo results expose host mbedtls internals only as opaque pointers; guest code cannot operate on them.
