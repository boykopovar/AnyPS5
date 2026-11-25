# libSceNKWebKit

## add Functional

- [WebCore::NetworkStorageSession cookie storage](../../../core/libs/prx/libSceNKWebKit/src/SessionCookies.cpp) (libSceNKWebKit) - host-memory jars approximate functional URL lookup using [Soup](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/soup/NetworkStorageSessionSoup.cpp). The pinned PlayStation build uses [Curl](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/curl/NetworkStorageSessionCurl.cpp), where URL lookup is still a stub and batch storage ignores the origin. This jar checks the origin domain and secure scheme, and omits `commentURL` until URL ownership is modeled. Session construction and destruction still throw, so jars survive address reuse. Public suffixes, acceptance policy, partitioning and non-ASCII case folding are unverified. Closes when: compared with a console module.

## remove `_ZN7WebCore21NetworkStorageSession10getCookiesERKN3WTF3URLE`

## remove `_ZN7WebCore21NetworkStorageSession10setCookiesERKN3WTF6VectorINS_6CookieELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEERKNS1_3URLESB_`
