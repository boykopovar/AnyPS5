# libcurl

## add Functional

- [Options that name files](../../../core/libs/prx/libcurl/Export.cpp) (libcurl) - files curl writes for `CURLOPT_COOKIEJAR`, `CURLOPT_HSTS` and `CURLOPT_ALTSVC` are not recorded as written guest paths, and on Windows the backend opens these files through the ANSI API, so a host path outside the active code page cannot be used. Only `COOKIEFILE`, `COOKIEJAR`, `NETRC_FILE` and `CAINFO` are run by a test. `CURLOPT_UNIX_SOCKET_PATH`, `CURLOPT_ABSTRACT_UNIX_SOCKET`, `CURLOPT_RANDOM_FILE`, `CURLOPT_EGDSOCKET` and the SSH key options still take host paths. `CAINFO`, `CAPATH`, `CRLFILE`, `ISSUERCERT`, `SSLCERT`, `SSLKEY`, `PINNEDPUBLICKEY` and their `PROXY_` forms, `COOKIEFILE`, `COOKIEJAR`, `NETRC_FILE`, `HSTS` and `ALTSVC` are resolved like `fopen` when set; an empty name, `-` for the cookie options and a `sha256//` pin pass unchanged, and a path that does not resolve becomes an empty name.
