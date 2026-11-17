# libSceNKWebKit

## add Additional callable exports

- [WebCore deprecated global settings](../../../core/libs/prx/libSceNKWebKit/src/DeprecatedGlobalSettings.cpp) (libSceNKWebKit) - the scrollbar, scroll animator, CSS priority, low-power buffer, resource load statistics and SSL certificate setters and `mockScrollbarsEnabled` store process-wide flags that start false, as in the pinned [`DeprecatedGlobalSettings.cpp`](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/DeprecatedGlobalSettings.cpp) and the older out-of-line form in WebKit [`c55364d53f97`](https://github.com/WebKit/WebKit/blob/c55364d53f97/Source/WebCore/page/DeprecatedGlobalSettings.cpp). No other export reads them yet, so they change no page, scrollbar or network behaviour.

## remove `_ZN7WebCore24DeprecatedGlobalSettings24setMockScrollbarsEnabledEb`

## remove `_ZN7WebCore24DeprecatedGlobalSettings24setUsesOverlayScrollbarsEb`

## remove `_ZN7WebCore24DeprecatedGlobalSettings26setAllowsAnySSLCertificateEb`
