# libSceNKWebKit

## add Functional

- [WebCore::ResourceLoadStatistics](../../../core/libs/prx/libSceNKWebKit/src/ResourceLoadStatistics.cpp) (libSceNKWebKit) - not compared with a console module. `merge`, `decode`, `encode` and `toString` follow pinned [ResourceLoadStatistics.cpp:89–564](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/loader/ResourceLoadStatistics.cpp#L89-L564), with the registrable-domain sets using the pinned [HashTable.h:871–947](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/HashTable.h#L871-L947) `add` and `rehash` 1252–1291. The struct omits the WEB_API_STATISTICS members, which are only defined for Cocoa. The `merge` domain assertion is treated as disabled, `decodeObjects`' discarded `Vector<String>` is not materialized, and an empty decoded domain becomes a fresh `nullOrigin` string rather than the static literal. Closes when: compared with a console module.

## remove | `_ZN7WebCore22ResourceLoadStatistics5mergeERKS0_` | `Abv1Cb-pj2w` |

## remove | `_ZN7WebCore22ResourceLoadStatistics6decodeERNS_12KeyedDecoderEj` | `9D40kL7Jyes` |

## remove | `_ZNK7WebCore22ResourceLoadStatistics6encodeERNS_12KeyedEncoderE` | `eFrm70bYHn0` |

## remove | `_ZNK7WebCore22ResourceLoadStatistics8toStringEv` | `Vffv7VcV358` |
