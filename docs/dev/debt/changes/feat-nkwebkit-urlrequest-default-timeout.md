# libSceNKWebKit

## change [WebKit URLRequest API]

- [WebKit URLRequest API](../../../core/libs/prx/libSceNKWebKit/src/URLRequest.cpp) (libSceNKWebKit) - `WKURLRequest*` functions follow WebKit's C API behavior: `WKURLRequestCreateWithWKURL` wraps the URL in an API object, `WKURLRequestCopyURL` returns a retained URL copy, `WKURLRequestCopyFirstPartyForCookies` returns null for initial requests per WebKit's default `ResourceRequest` state, `WKURLRequestCopyHTTPMethod` returns an immutable string (defaulting to `"GET"`), and `WKURLRequestCopySettingHTTPBody` creates a new request copy with updated body data.

## add Functional

- [WKURLRequestSetDefaultTimeoutInterval](../../../core/libs/prx/libSceNKWebKit/src/ResourceRequest.cpp) (libSceNKWebKit) - the pinned source also passes the value to every `WebProcessPool` ([APIURLRequest.cpp:46-52](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebKit/Shared/API/APIURLRequest.cpp#L46-L52)), which is not modelled; only the `ResourceRequestBase` default timeout, initially 0, is set, as [WKURLRequest.cpp:66-69](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebKit/Shared/API/c/WKURLRequest.cpp#L66-L69) does through `API::URLRequest::setDefaultTimeoutInterval`. The PS5 ABI is unverified.

## remove | `WKURLRequestSetDefaultTimeoutInterval` | `YXTIbI-WYtE` |
