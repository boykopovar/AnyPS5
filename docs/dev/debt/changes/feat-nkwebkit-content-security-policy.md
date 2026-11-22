# libSceNKWebKit

## add Functional

- [WebCore::ContentSecurityPolicy](../../../core/libs/prx/libSceNKWebKit/src/ContentSecurityPolicy.cpp) (libSceNKWebKit) - not compared with a console module. Its constructor is only in the 3.20 inventory, so the object, its arguments and what its client receives follow the 2020 headers at [WebKit 73a35b43](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/page/csp/ContentSecurityPolicy.h): 176-byte object, 64-byte `SecurityOrigin`, 24-byte `CSPInfo`, 80-byte violation event `Init`, 80-byte `FormData` with a `WTF::Variant` element. `m_selfSource` and `m_policies` hold host objects. Origins follow the 2020 `SecurityOrigin::create`, without the blob registry's cached origins or the origin access allow lists, whose exports throw. Releasing a report whose elements became files or blobs throws. `ContentSecurityPolicyResponseHeaders(const ResourceResponse&)` still throws: one name for two `ResourceResponse` layouts.
- [Content Security Policy parsing and matching](../../../core/libs/prx/libSceNKWebKit/src/ContentSecurityPolicyParser.cpp) (libSceNKWebKit) - follows the directive list, source list, source, media list and `parseSandboxPolicy` of [WebKit 73a35b43](https://github.com/WebKit/WebKit/tree/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/page/csp), not a console module. `manifest-src` is reported as unrecognized: `ENABLE_APPLICATION_MANIFEST` follows `ENABLE_EXPERIMENTAL_FEATURES`, off by default in `OptionsPlayStation.cmake`, and the console build setting is unknown. Percent-encoded paths are decoded as UTF-8 the way `TextCodecUTF8` does; no other encoding is modelled. The `data:` and `blob:` allowances of `*` for `img-src` and `media-src`, and the eval and WebAssembly error messages, are not written: no export reaches them. Only policies from HTTP headers are parsed, the one origin the exports can give.
- [ContentSecurityPolicy 3.20 limits](../../../core/libs/prx/libSceNKWebKit/src/ContentSecurityPolicyAbi.hpp) (libSceNKWebKit) - the seven names shared with the 11.x inventory read the 2020 layout only: the 11.x object has further members (`m_documentURL`, `m_isHeaderDelivered`, `m_contentSecurityPolicyModeForExtension`) and would be misread, but no export creates one. Directives that 11.x added, such as `worker-src`, `prefetch-src` and `report-to`, are reported as unrecognized, as in 2020. The `ResourceRequest` overload of `upgradeInsecureRequestIfNeeded` throws `std::logic_error` when the request's `m_resourceRequestUpdated` flag is clear, as [ResourceRequest.cpp](../../../core/libs/prx/libSceNKWebKit/src/ResourceRequest.cpp) does: `doUpdateResourceRequest` is not implemented.

## remove `_ZN7WebCore21ContentSecurityPolicy17didReceiveHeadersERKNS_36ContentSecurityPolicyResponseHeadersEON3WTF6StringENS0_19ReportParsingErrorsE`

## remove `_ZN7WebCore21ContentSecurityPolicyD1Ev`

## remove `_ZN7WebCore21ContentSecurityPolicyD2Ev`

## remove `_ZNK7WebCore21ContentSecurityPolicy15responseHeadersEv`

## remove `_ZNK7WebCore21ContentSecurityPolicy22overridesXFrameOptionsEv`

## remove `_ZNK7WebCore21ContentSecurityPolicy30upgradeInsecureRequestIfNeededERN3WTF3URLENS0_19InsecureRequestTypeE`

## remove `_ZNK7WebCore21ContentSecurityPolicy30upgradeInsecureRequestIfNeededERNS_15ResourceRequestENS0_19InsecureRequestTypeE`
