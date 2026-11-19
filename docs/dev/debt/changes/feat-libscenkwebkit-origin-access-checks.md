# libSceNKWebKit

## add Functional

- [WebCore::SecurityOrigin access checks](../../../core/libs/prx/libSceNKWebKit/src/SecurityOriginAccess.cpp) (libSceNKWebKit) - not compared with a console module. `canRequest` and `canDisplay` throw `NotImplemented_nid_no_patch` for `blob:` URLs (no blob origin cache), and `canDisplay` for a `file` origin with a `file:` URL (`FileSystem::filesHaveSameVolume` is a stub). `canAccessStorage` still throws: the 11.x layout has no `m_storageBlockingPolicy`. These 3.20 exports follow [SecurityOrigin.cpp](https://github.com/WebKit/WebKit/blob/7dba47c0816064cba98c0aae6bb92ffd683eac85/Source/WebCore/page/SecurityOrigin.cpp) of February 2021 on the 11.x layout, reading `isUnique()` as the opaque `SecurityOriginData` index; whether the 3.20 object matches that layout is unverified. Closes when: compared with a 3.20 console module.
- [WebCore::SecurityPolicy origin access allowlist](../../../core/libs/prx/libSceNKWebKit/src/SecurityPolicy.cpp) (libSceNKWebKit) - not compared with a console module. `allowAccessTo` patterns are not modelled, as that export throws, and the `WKBundle` allowlist exports still throw. Follows the 11.x [SecurityPolicy.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/SecurityPolicy.cpp): a host-side list keyed by the source `SecurityOriginData`, read by `canRequest` and `canDisplay`; 2021 WebKit keys it by `toString`, which differs for `file` origins that enforce path separation. Closes when: compared with a console module.

## remove `_ZN7WebCore14SecurityPolicy27resetOriginAccessAllowlistsEv`

## remove `_ZN7WebCore14SecurityPolicy29addOriginAccessAllowlistEntryERKNS_14SecurityOriginERKN3WTF6StringES7_b`

## remove `_ZN7WebCore14SecurityPolicy32removeOriginAccessAllowlistEntryERKNS_14SecurityOriginERKN3WTF6StringES7_b`
