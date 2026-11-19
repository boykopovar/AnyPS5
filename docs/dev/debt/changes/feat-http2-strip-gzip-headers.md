# libSceHttp2

## remove `mMWZm0+7XHM`

## change [libSceHttp2 gzip responses]

- [libSceHttp2 gzip responses](../../../core/libs/prx/libSceHttp2/Transport.cpp) - two differences from a retail PS5 on firmware 13.60, measured over HTTP/1.1 in a homebrew payload against the system `libSceHttp2.sprx` (log in the pull request that added `sceHttp2SetStripGZIPHeadersEnabled`): a new template inflates gzip responses there, while here inflation starts disabled; and with inflation on, `sceHttp2GetResponseContentLength` reports kind 1 and length 0 for a gzip response with a `Content-Length` there, while here it reports kind 0 and the inflated length. Console buffering and error mappings are unverified. Enabled gzip responses wait for END_STREAM and use 64 MiB encoded/decoded host limits. The guest supplies Accept-Encoding. Other content encodings are not inflated

## add Functional

- [sceHttp2SetStripGZIPHeadersEnabled](../../../core/libs/prx/libSceHttp2/Export.cpp) (libSceHttp2) - differs from a retail PS5 on firmware 13.60, measured over HTTP/1.x in a homebrew payload against the system `libSceHttp2.sprx` (log in the pull request): setting it on a request after `sceHttp2SendRequest` returns 0 there and throws here, `sceHttp2GetAllResponseHeaders` before a send returns `0x817B1065` there and throws here, and the console's header size counts the terminating NUL, stripped or not. Stripping HTTP/2 headers is unmeasured; the module's `sceHttp2GetAllResponseHeaders` (`0x1d930`) has no version check. The `0x817B1076` stack check below 0x1FD0 bytes of stack is not reproduced. Closes when: measured on a PS5 over HTTP/2
