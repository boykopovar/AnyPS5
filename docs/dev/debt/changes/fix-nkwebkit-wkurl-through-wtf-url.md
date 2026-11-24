# libSceNKWebKit

## change [WKURL URL component and comparison helpers]

- [WKURL URL component and comparison helpers](../../../core/libs/prx/libSceNKWebKit/src/URL.cpp) (libSceNKWebKit) - `WKURLCopyHostName`, `WKURLCopyPath`, `WKURLCopyScheme`, `WKURLCopyLastPathComponent`, `WKURLCreateWithBaseURL` and `WKURLIsEqual` read this module's `WTF::URL` parser, as `API::URL` does in [the pinned WebKit source](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebKit/Shared/API/APIURL.h), so that parser's gaps apply to them: a URL with a non-ASCII host throws until `uidna_openUTS46` is implemented. `WKURLCreateWithUTF8CString` keeps the string as given, and a null string is kept as an empty one. Console execution is unverified.
