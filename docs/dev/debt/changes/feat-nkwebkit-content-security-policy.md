# libSceNKWebKit

## add Functional

- [WebCore::ContentSecurityPolicy](../../../core/libs/prx/libSceNKWebKit/src/ContentSecurityPolicy.cpp) (libSceNKWebKit) - not compared with a console module. Its constructor is only in the 3.20 inventory, so the object and its arguments follow the 2020 [ContentSecurityPolicy.h](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/page/csp/ContentSecurityPolicy.h): 176 bytes, a `WTF::Optional` header cache, a `HashSet` table with four counters before its entries, and the `ResourceRequest` of `LegacyResourceRequestAbi.hpp`. `m_selfSource` and `m_policies` hold host objects. The self source comes from the module's 11.x `SecurityOrigin::create`. Policy headers come back as 8-bit strings when every unit fits. `allowScriptFromSource`, `allowConnectToSource`, `allowChildContextFromSource`, `allowFrameAncestors` and `ContentSecurityPolicyResponseHeaders(const ResourceResponse&)` still throw; the last is one name for two `ResourceResponse` layouts.
- [Content Security Policy parsing](../../../core/libs/prx/libSceNKWebKit/src/ContentSecurityPolicyParser.cpp) (libSceNKWebKit) - follows the directive list, source list, media list and `parseSandboxPolicy` of [WebKit 73a35b43](https://github.com/WebKit/WebKit/tree/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/page/csp), not a console module. `manifest-src` is reported as unrecognized: `ENABLE_APPLICATION_MANIFEST` follows `ENABLE_EXPERIMENTAL_FEATURES`, off by default in `OptionsPlayStation.cmake`, and the console build setting is unknown. Source paths are kept as written; percent-decoding belongs with URL matching, which is not implemented. The eval and WebAssembly error messages of a policy are not built: only a script execution context reads them. Only policies from HTTP headers are parsed, the one origin the exports can give.
- [ContentSecurityPolicy 3.20 limits](../../../core/libs/prx/libSceNKWebKit/src/ContentSecurityPolicyAbi.hpp) (libSceNKWebKit) - the seven names shared with the 11.x inventory read the 2020 layout only: the 11.x object has further members (`m_documentURL`, `m_isHeaderDelivered`, `m_contentSecurityPolicyModeForExtension`) and would be misread, but no export creates one. Directives that 11.x added, such as `worker-src`, `prefetch-src` and `report-to`, are reported as unrecognized, as in 2020. The `ResourceRequest` overload of `upgradeInsecureRequestIfNeeded` throws `std::logic_error` when the request's `m_resourceRequestUpdated` flag is clear, as [ResourceRequest.cpp](../../../core/libs/prx/libSceNKWebKit/src/ResourceRequest.cpp) does: `doUpdateResourceRequest` is not implemented.

## remove `_ZN7WebCore21ContentSecurityPolicy17didReceiveHeadersERKNS_36ContentSecurityPolicyResponseHeadersEON3WTF6StringENS0_19ReportParsingErrorsE`

## remove `_ZN7WebCore21ContentSecurityPolicyD1Ev`

## remove `_ZN7WebCore21ContentSecurityPolicyD2Ev`

## remove `_ZNK7WebCore21ContentSecurityPolicy15responseHeadersEv`

## remove `_ZNK7WebCore21ContentSecurityPolicy22overridesXFrameOptionsEv`

## remove `_ZNK7WebCore21ContentSecurityPolicy30upgradeInsecureRequestIfNeededERN3WTF3URLENS0_19InsecureRequestTypeE`

## remove `_ZNK7WebCore21ContentSecurityPolicy30upgradeInsecureRequestIfNeededERNS_15ResourceRequestENS0_19InsecureRequestTypeE`
