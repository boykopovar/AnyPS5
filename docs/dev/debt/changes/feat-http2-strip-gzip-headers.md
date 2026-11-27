# libSceHttp2

## remove `mMWZm0+7XHM`

## change [libSceHttp2 gzip responses]

- [libSceHttp2 gzip responses](../../../core/libs/prx/libSceHttp2/Transport.cpp) - enabled gzip responses wait for END_STREAM before the first byte is read and use 64 MiB encoded/decoded host limits, while a retail PS5 on firmware 13.60 reports no length for them; console buffering and error mappings are unverified. New templates inflate gzip responses, as on that PS5, measured over HTTP/1.x in a homebrew payload against the system `libSceHttp2.sprx` (logs in the pull requests that added `sceHttp2SetStripGZIPHeadersEnabled` and made templates inflate by default). A response is gzip when its first `Content-Encoding` value starts with `gzip`, ignoring case, as on that PS5. The guest supplies Accept-Encoding; that PS5 sends none of its own. Other content encodings, `deflate` among them, are not inflated, as on that PS5

## add Functional

- [sceHttp2SetStripGZIPHeadersEnabled](../../../core/libs/prx/libSceHttp2/Export.cpp) (libSceHttp2) - follows a retail PS5 on firmware 13.60, measured over HTTP/1.x in a homebrew payload against the system `libSceHttp2.sprx` ([log](https://github.com/boykopovar/AnyPS5/pull/23118#issuecomment-6988415062)): on a request after `sceHttp2SendRequest` it returns 0 and changes nothing, `sceHttp2GetAllResponseHeaders` before a send returns `0x817B1065`, and the HTTP/1.x header size counts the terminating NUL, stripped or not. Over HTTP/2 the size leaves the NUL out and stripping is unmeasured, though the module's `sceHttp2GetAllResponseHeaders` (`0x1d930`) has no version check. The `0x817B1076` stack check below 0x1FD0 bytes of stack is not reproduced. Closes when: measured on a PS5 over HTTP/2

## add Functional

- [sceHttp2GetResponseContentLength](../../../core/libs/prx/libSceHttp2/Transport.cpp) (libSceHttp2) - HTTP/2 answers are unmeasured; measured over HTTP/1.x on a retail PS5 on firmware 13.60 in a homebrew payload against the system `libSceHttp2.sprx` (log in the pull request that made them follow it), whose `sceHttp2GetResponseContentLength` (`0x1d850`) has no version check. Before a send it returns `0x817B1065` and leaves both outputs; a gzip response with a `Content-Length` reports kind 1 and length 0 when inflation is on, or for any title whose compiled SDK version is below 0x12500000 (a failed lookup counts as 0x99900000), and the answer does not change once the body is read. The SDK branch is read from the module, not measured: the payload's SDK was 0x13590001. A request still being sent asynchronously returns `0x817B1065` in the module (state other than 5); that case is unmeasured. Closes when: measured on a PS5 over HTTP/2 and from a title below SDK 0x12500000

## change [sceHttp2GetAllResponseHeaders]

- [sceHttp2GetAllResponseHeaders](../../../core/libs/prx/libSceHttp2/Export.cpp) (libSceHttp2) - HTTP/1.x returns the final raw header block, its size counting the terminating NUL as a retail PS5 on 13.60 does (see sceHttp2SetStripGZIPHeadersEnabled); its lifetime contract remains unverified. HTTP/2 serializes a decoded header block as `HTTP/2.0 <code> HTTP2 NO PHRASE\r\n`, then one `<name>: <value>\r\n` line per header with the lowercase name nghttp2 reports, a consumed `:status` pseudo-header and a final `\r\n`; the format comes from the firmware 11.00 `libSceHttp2.prx` (`sceHttp2GetAllResponseHeaders` at `0x15e0`, helper at `0x5210` and its serializer), so the reason phrase is the console's literal and not a local table. Status and Content-Length fields are decoded; chunked HTTP/1.x reports kind 2, absent Content-Length kind 1, following SharpProspero. The buffer lives until the request is deleted.
