# libSceNKWebKit

## add Unknown function info

- [WebCore static value helpers](../../../core/libs/prx/libSceNKWebKit/src/WebCoreValueHelpers.cpp) (libSceNKWebKit) - not compared with a console module; the pinned tree is newer than the exports. `reduceTimeResolution` floors to 5 seconds ([ResourceLoadStatistics.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/loader/ResourceLoadStatistics.cpp#L566-L569)), `platformDefaultMediaSourceEnabled` is `true` ([SettingsBase.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/SettingsBase.cpp#L87-L91)) and `scriptNameToCode` uses the table of [LocaleToScriptMapping.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/text/LocaleToScriptMapping.cpp#L40-L158) with ICU `uscript.h` values, matching four ASCII characters ignoring case. Closes when: compared with a console module.

## add Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceNKWebKit/Export.cpp) - 5929 of the 8703 inventory declarations listed below throw through `NotImplemented_nid_no_patch`; signatures and behavior are unverified. The later 13.60-labelled image contains all 8703; 8448 match the public 3.20 catalogue, while 255 are absent and their presence in retail 3.20 is unverified. Per-image records show the latter in a separate 3.20-labelled image, but neither image label is authenticated. Identified names match NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv). Data exports need separate analysis.

## remove `_ZN7WebCore12SettingsBase33platformDefaultMediaSourceEnabledEv`

## remove `_ZN7WebCore22ResourceLoadStatistics20reduceTimeResolutionEN3WTF8WallTimeE`
