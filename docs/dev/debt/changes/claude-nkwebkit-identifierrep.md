# libSceNKWebKit

## add Functional

- [WebCore::IdentifierRep registry](../../../core/libs/prx/libSceNKWebKit/src/IdentifierRep.cpp) (libSceNKWebKit) - not compared with a console module. The 16-byte layout in [IdentifierRepAbi.hpp](../../../core/libs/prx/libSceNKWebKit/src/IdentifierRepAbi.hpp) follows [IdentifierRep.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/bridge/IdentifierRep.h#L42-L66); `get` and `isValid` follow [IdentifierRep.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/bridge/IdentifierRep.cpp#L54-L109): 0 and -1 are singletons that `isValid` rejects, identifiers are never freed, and strings are keyed by their decoded text but keep the first caller's bytes. A host mutex serializes the registry. Closes when: compared with a console module.

## add Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceNKWebKit/Export.cpp) - 5936 of the 8703 inventory declarations listed below throw through `NotImplemented_nid_no_patch`; signatures and behavior are unverified. The later 13.60-labelled image contains all 8703; 8448 match the public 3.20 catalogue, while 255 are absent and their presence in retail 3.20 is unverified. Per-image records show the latter in a separate 3.20-labelled image, but neither image label is authenticated. Identified names match NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv). Data exports need separate analysis.

## remove `_ZN7WebCore13IdentifierRep3getEPKc`

## remove `_ZN7WebCore13IdentifierRep3getEi`

## remove `_ZN7WebCore13IdentifierRep7isValidEPS0_`
