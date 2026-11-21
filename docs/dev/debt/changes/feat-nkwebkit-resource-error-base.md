# libSceNKWebKit

## add Unknown function info

- [WebCore::ResourceErrorBase exports](../../../core/libs/prx/libSceNKWebKit/src/ResourceError.cpp) (libSceNKWebKit) - not compared with a console module. The 80-byte `ResourceError` layout is the one in [ResourceErrorAbi.hpp](../../../core/libs/prx/libSceNKWebKit/src/ResourceErrorAbi.hpp) (pinned ResourceErrorBase.h:101-106 plus the Curl members of curl/ResourceError.h:68-69); its members are raw `StringImpl` pointers, and `isolatedCopy` returns one owned reference per string. `setType`, `lazyInit` and `isolatedCopy` follow [ResourceErrorBase.cpp:43-69](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/ResourceErrorBase.cpp#L43-L69): Curl `ResourceError` does not shadow `platformLazyInit`, so `lazyInit` is a no-op, and `isolatedCopy` leaves `m_isSanitized` unset [ResourceErrorCurl.cpp:51-54](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/curl/ResourceErrorCurl.cpp#L51-L54). Strings copy through the existing `String::isolatedCopy` export. Null `this` throws for all three. Closes when: compared with a console module.

## remove | `_ZN7WebCore17ResourceErrorBase7setTypeENS0_4TypeE` | `O9YsW44maCs` |

## remove | `_ZNK7WebCore17ResourceErrorBase12isolatedCopyEv` | `RIXLOUDwuUg` |

## remove | `_ZNK7WebCore17ResourceErrorBase8lazyInitEv` | `TasWJCEWUlI` |
