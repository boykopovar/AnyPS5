# libSceNKWebKit

## add Additional callable exports

- [WebCore deprecated global settings](../../../core/libs/prx/libSceNKWebKit/src/DeprecatedGlobalSettings.cpp) (libSceNKWebKit) - the scrollbar, scroll animator, CSS priority, low-power buffer, resource load statistics and SSL certificate setters and `mockScrollbarsEnabled` store process-wide flags that start false, as in the pinned [`DeprecatedGlobalSettings.cpp`](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/DeprecatedGlobalSettings.cpp) and the older out-of-line form in WebKit [`c55364d53f97`](https://github.com/WebKit/WebKit/blob/c55364d53f97/Source/WebCore/page/DeprecatedGlobalSettings.cpp). No other export reads them yet, so they change no page, scrollbar or network behaviour.

- [Additional callable exports](../../../core/libs/prx/libSceNKWebKit/Export.cpp) - 5936 of the 8703 inventory declarations listed below throw through `NotImplemented_nid_no_patch`; signatures and behavior are unverified. The later 13.60-labelled image contains all 8703; 8448 match the public 3.20 catalogue, while 255 are absent and their presence in retail 3.20 is unverified. Per-image records show the latter in a separate 3.20-labelled image, but neither image label is authenticated. Identified names match NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv). Data exports need separate analysis.

## remove `_ZN7WebCore24DeprecatedGlobalSettings24setMockScrollbarsEnabledEb`

## remove `_ZN7WebCore24DeprecatedGlobalSettings24setUsesOverlayScrollbarsEb`

## remove `_ZN7WebCore24DeprecatedGlobalSettings26setAllowsAnySSLCertificateEb`
