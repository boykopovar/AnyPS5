# libSceNKWebKit

## add Unknown function info

- [WebCore page strings](../../../core/libs/prx/libSceNKWebKit/src/WebCorePageStrings.cpp) (libSceNKWebKit) - unverified on a console; returns fresh 8-bit strings, not literal-backed ones. Follows [DisabledAdaptations.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/DisabledAdaptations.cpp#L33-L36) and [PerformanceLoggingClient.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/PerformanceLoggingClient.cpp#L31-L45). `standardUserAgent` and `standardUserAgentForURL` still throw: [UserAgentPlayStation.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/playstation/UserAgentPlayStation.cpp#L41-L59) reads name and version via unimplemented `getenv_np`; consoles report `PlayStation 5/<version>`. Closes when: compared with a console module.

## add Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceNKWebKit/Export.cpp) - 5930 of the 8703 inventory declarations listed below throw through `NotImplemented_nid_no_patch`; signatures and behavior are unverified. The later 13.60-labelled image contains all 8703; 8448 match the public 3.20 catalogue, while 255 are absent and their presence in retail 3.20 is unverified. Per-image records show the latter in a separate 3.20-labelled image, but neither image label is authenticated. Identified names match NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv). Data exports need separate analysis.

## remove `_ZN7WebCore19watchAdaptationNameEv`

## remove `_ZN7WebCore24PerformanceLoggingClient35synchronousScrollingReasonsAsStringEN3WTF9OptionSetINS_26SynchronousScrollingReasonEEE`
