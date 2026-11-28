# libSceNKWebKit

## add Functional

- [WebCore::IdentifierRep registry](../../../core/libs/prx/libSceNKWebKit/src/IdentifierRep.cpp) (libSceNKWebKit) - not compared with a console module. The 16-byte layout in [IdentifierRepAbi.hpp](../../../core/libs/prx/libSceNKWebKit/src/IdentifierRepAbi.hpp) follows [IdentifierRep.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/bridge/IdentifierRep.h#L42-L66); `get` and `isValid` follow [IdentifierRep.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/bridge/IdentifierRep.cpp#L54-L109): 0 and -1 are singletons that `isValid` rejects, identifiers are never freed, and strings are keyed by their decoded text but keep the first caller's bytes. A host mutex serializes the registry. Closes when: compared with a console module.

## remove `_ZN7WebCore13IdentifierRep3getEPKc`

## remove `_ZN7WebCore13IdentifierRep3getEi`

## remove `_ZN7WebCore13IdentifierRep7isValidEPS0_`
