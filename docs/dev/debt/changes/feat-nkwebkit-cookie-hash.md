# libSceNKWebKit

## add Functional

- [WebCore::Cookie hash and operator==](../../../core/libs/prx/libSceNKWebKit/src/WebCoreCookie.cpp) (libSceNKWebKit) - not compared with a console module. They follow the non-Cocoa definitions of pinned [Cookie.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531aeddc11259e8515671cf5188e038d/Source/WebCore/platform/network/Cookie.cpp#L31-L44), unchanged in the PS5 11.x source, on the 136-byte layout of [Cookie.h](https://github.com/WebKit/WebKit/blob/e12a212a531aeddc11259e8515671cf5188e038d/Source/WebCore/platform/Cookie.h#L83-L98) (`domain` at byte 16, `path` at 24, `secure` at 57): equality compares name, domain, path and `secure`, and the hash adds the three string hashes and `secure` modulo 2^32. A null name, domain or path in `hash`, where `StringHash` faults, and a null cookie throw. Closes when: compared with a console module.

## remove | `_ZNK7WebCore6Cookie4hashEv` | `skipUpdQW1w` |

## remove | `_ZNK7WebCore6CookieeqERKS0_` | `1FST2EMhfHk` |
