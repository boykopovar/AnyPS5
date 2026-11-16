# libSceNKWebKit

## add Unknown function info

- [WebCore::ResourceResponse lifetime](../../../core/libs/prx/libSceNKWebKit/src/ResourceResponse.cpp) - not compared with a console module. The special members are the implicit memberwise ones: vectors copy at capacity equal to size, assignment preserves capacity when shrinking, strings retain through the +2 convention. A `Box<NetworkLoadMetrics>` last release that would reach 0 throws when the payload at offset 8 is non-null, before changing anything, because the console runs `~NetworkLoadMetrics` (releasing the `protocol` string and the inspector-metrics reference) before `fastFree`; a real box stores the payload inline and its leading doubles are non-zero even default, so real guest boxes always throw; only a zeroed payload frees. `m_initLevel` is zeroed where the curl constructors leave it indeterminate; the pinned `lazyInit` ignores it. Closes when: compared with a console module.

- [WebCore::ResourceResponse::platformSuggestedFilename](../../../core/libs/prx/libSceNKWebKit/src/ResourceResponse.cpp) - not compared with a console module. `platformSuggestedFilename` returns the Content-Disposition filename via `filenameFromHTTPContentDisposition`. `cfURLResponse` stays a stub: the symbol is only in the older 3.20 inventory and the pinned 11.x source has no such member, so its return type is unknown. Closes when: compared with a console module.

## change [WebCore::ResourceResponseBase](../../../core/libs/prx/libSceNKWebKit/src/ResourceResponse.cpp)

- [WebCore::ResourceResponseBase](../../../core/libs/prx/libSceNKWebKit/src/ResourceResponse.cpp) - guest layout and header ordinals are unverified on console. The 280-byte layout now extends past the 32-byte header map at 80 through the `Box<NetworkLoadMetrics>` pointer, the PlayStation `CertificateInfo` optional, the status code, the state and parsed flag bytes and the marked times, pinned with `static_assert`. These exports assume curl's no-op lazy initialization. Cache and date getters parse live headers without writing the marked times and parsed flags upstream keeps. The setters take `const String&` per the module signatures, not the pinned header's AtomString ones. The console assigns through `AtomStringImpl::add`, so the setters and field constructors throw unless the impl is null or already an atom (`s_hashFlagStringKindIsAtom`); an empty non-atom string, which the console maps to the shared empty atom, also throws.

## change [Resource response time and filename getters](../../../core/libs/prx/libSceNKWebKit/src/ResourceResponse.cpp)

- [Resource response time and filename getters](../../../core/libs/prx/libSceNKWebKit/src/ResourceResponse.cpp) - the six legacy time returns are unverified on console. The pinned 11.x getters return `std::optional<Seconds>` and `std::optional<WallTime>` ([ResourceResponseBase.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/ResourceResponseBase.cpp#L736-L766)), trivially copyable and would return in registers, while the model follows the historical [WTF::Optional](https://github.com/WebKit/WebKit/blob/e12a212a531a/Source/WTF/wtf/Optional.h) indirect return (engagement at byte 0, seconds at byte 8). [PlayStation's filename parser](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/HTTPParsers.cpp) matches filename keys case-sensitively, splits at semicolons inside quotes and returns an empty string for an empty parameter.

## remove | `_ZN7WebCore20ResourceResponseBaseC1ERKN3WTF3URLERKNS1_6StringExS7_` | `xacsaU-hKOE` |

## remove | `_ZN7WebCore20ResourceResponseBaseC2ERKN3WTF3URLERKNS1_6StringExS7_` | `28-zrObSsN8` |
