# libSceNKWebKit

## remove `_ZN7WebCore25updateRedirectChainStatusERNS_24RedirectChainCacheStatusERKNS_16ResourceResponseE`

## add Functional

- [WebCore::updateRedirectChainStatus](../../../core/libs/prx/libSceNKWebKit/src/RedirectChainStatus.cpp) (libSceNKWebKit) - not compared with a console module. It follows the pinned [CacheValidation.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/CacheValidation.cpp#L150-L164). A cached redirect calls the `computeFreshnessLifetimeForHTTPFamily` and `computeCurrentAge` exports, which still throw on main (PRs 19830 and 19719 implement them), so only the not-cached paths run today. Closes when: both helpers land and the result is compared with a console module.
