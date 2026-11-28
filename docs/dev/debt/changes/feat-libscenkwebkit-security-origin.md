# libSceNKWebKit

## change [WebCore::SecurityOrigin]

- [WebCore::SecurityOrigin](../../../core/libs/prx/libSceNKWebKit/src/SecurityOrigin.cpp) (libSceNKWebKit) - not compared with a console module. [SecurityOriginAbi.hpp](../../../core/libs/prx/libSceNKWebKit/src/SecurityOriginAbi.hpp) follows the pinned [SecurityOrigin.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/SecurityOrigin.h) release layout and assumes libc++'s ABI v1 `std::variant` with a 32-bit index. `create(URL)` throws `NotImplemented_nid_no_patch` for `blob:` URLs, having no blob origin cache. No export registers scheme-handler schemes, so other non-special schemes give opaque origins. Without a URL parser, `SecurityOriginData::securityOrigin` gives a `file` origin the path `/` and throws for other local schemes. The `StringView` by-value convention of `isLocalHostOrLoopbackIPAddress` is unverified.
