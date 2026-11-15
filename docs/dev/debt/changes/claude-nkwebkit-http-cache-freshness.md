# libSceNKWebKit

## remove [WebCore::ResourceResponseBase]

## add Functional

- [WebCore::ResourceResponseBase](../../../core/libs/prx/libSceNKWebKit/src/ResourceResponse.cpp) - guest layout and header ordinals are unverified on console. [Historical WebKit](https://github.com/WebKit/WebKit/blob/e12a212a531a/Source/WebCore/platform/network/ResourceResponseBase.h) and the [PlayStation source](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/ResourceResponseBase.h) agree through the 32-byte header map at offset 80. These exports assume curl's no-op lazy initialization. Cache and date getters parse live headers without writing fields beyond the known prefix. Metrics at 112, the 56-byte optional certificate at 120 and the status code at 176 come from source only. Flags, AtomString creation, certificate contents and the rest of the layout remain unresolved.

- [WebCore HTTP cache freshness and revalidation](../../../core/libs/prx/libSceNKWebKit/src/CacheValidation.cpp) (libSceNKWebKit) - `computeFreshnessLifetimeForHTTPFamily` and `updateResponseHeadersAfterRevalidation` follow the pinned [CacheValidation.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/CacheValidation.cpp#L44-L148) and are unmeasured on the console. Freshness reads the status code at the source-derived offset 176 and reuses the response getters, and assumes `Seconds` and `WallTime` pass as one double. Revalidation keeps the `content-security-` exception and the ignore lists of that tree; the later upstream changes to invalid `Expires`, explicit freshness and `Content-*` copying are not in it. Parsed-header flags are not reset because the getters parse live headers. `updateRedirectChainStatus` stays a stub until `computeCurrentAge` lands.

## remove `_ZN7WebCore37computeFreshnessLifetimeForHTTPFamilyERKNS_16ResourceResponseEN3WTF8WallTimeE`

## remove `_ZN7WebCore38updateResponseHeadersAfterRevalidationERNS_16ResourceResponseERKS0_`
