# libSceNKWebKit

## add Unknown function info

- [WebCore::CrossOriginAccessControlCheckDisabler](../../../core/libs/prx/libSceNKWebKit/src/CrossOriginAccessControl.cpp) (libSceNKWebKit) - not compared with a console module. The vtable pointer at 0, the enabled flag at 8 and the 16-byte size follow the pinned [CrossOriginAccessControl.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/loader/CrossOriginAccessControl.h#L79-L86) and the bodies follow [CrossOriginAccessControl.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/loader/CrossOriginAccessControl.cpp#L240-L257): the singleton starts enabled, copies copy the flag and assignment copies only the flag. The vtable's type-info slot is null, so guest RTTI on this class is unsupported. The deleting destructor frees through the guest global `operator delete`, since the class is not fast-allocated. Closes when: compared with a console module.

## add Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceNKWebKit/Export.cpp) - 5923 of the 8703 inventory declarations listed below throw through `NotImplemented_nid_no_patch`; signatures and behavior are unverified. The later 13.60-labelled image contains all 8703; 8448 match the public 3.20 catalogue, while 255 are absent and their presence in retail 3.20 is unverified. Per-image records show the latter in a separate 3.20-labelled image, but neither image label is authenticated. Identified names match NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv). Data exports need separate analysis.

## remove `_ZN7WebCore37CrossOriginAccessControlCheckDisabler39setCrossOriginAccessControlCheckEnabledEb`

## remove `_ZN7WebCore37CrossOriginAccessControlCheckDisabler9singletonEv`

## remove `_ZN7WebCore37CrossOriginAccessControlCheckDisablerC1ERKS0_`

## remove `_ZN7WebCore37CrossOriginAccessControlCheckDisablerC1Ev`

## remove `_ZN7WebCore37CrossOriginAccessControlCheckDisablerC2ERKS0_`

## remove `_ZN7WebCore37CrossOriginAccessControlCheckDisablerC2Ev`

## remove `_ZN7WebCore37CrossOriginAccessControlCheckDisablerD0Ev`

## remove `_ZN7WebCore37CrossOriginAccessControlCheckDisableraSERKS0_`

## remove `_ZNK7WebCore37CrossOriginAccessControlCheckDisabler36crossOriginAccessControlCheckEnabledEv`
