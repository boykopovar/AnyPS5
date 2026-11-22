# libSceNKWebKit

## add Functional

- [WebCore::NetworkStorageSession cookie storage](../../../core/libs/prx/libSceNKWebKit/src/SessionCookies.cpp) (libSceNKWebKit) - not compared with a console module: the layout and jar are source-derived.  `getCookies`, `setCookies` and `deleteCookie` keep cookies in one host-memory jar that follows [NetworkStorageSessionSoup.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/soup/NetworkStorageSessionSoup.cpp) and libsoup [soup-cookie.c](https://github.com/GNOME/libsoup/blob/2.68.4/libsoup/soup-cookie.c) with [soup-cookie-jar.c](https://github.com/GNOME/libsoup/blob/2.68.4/libsoup/soup-cookie-jar.c): host and leading-dot domain, path prefix, https and wss, expiry, replacement by name and path, and deletion through a past expiry. Public-suffix rejection, the accept policy, third-party partitioning and non-ASCII case folding are unverified. Closes when: compared with a console module.

## remove `_ZN7WebCore21NetworkStorageSession10getCookiesERKN3WTF3URLE`

## remove `_ZN7WebCore21NetworkStorageSession10setCookiesERKN3WTF6VectorINS_6CookieELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEERKNS1_3URLESB_`
