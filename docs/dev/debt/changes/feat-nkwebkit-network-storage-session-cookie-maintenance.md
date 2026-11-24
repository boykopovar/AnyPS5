# libSceNKWebKit

## add Functional

- [WebCore::NetworkStorageSession jar maintenance](../../../core/libs/prx/libSceNKWebKit/src/SessionCookieMaintenance.cpp) (libSceNKWebKit) - `getAllCookies` lists the calling session's unexpired cookies in insertion order. Expiry filtering and order are local choices with unverified console behavior. The parameterless `deleteAllCookies` and `flushCookieStore` signatures are inferred from their mangled names. The pinned Soup and Curl implementations take a completion handler for deletion and lack this flush export. Deletion clears only the calling session's jar, and flushing leaves cookies in memory without persisting persistent cookies. Closes when: compared with a console module.

## remove `_ZN7WebCore21NetworkStorageSession13getAllCookiesEv`
