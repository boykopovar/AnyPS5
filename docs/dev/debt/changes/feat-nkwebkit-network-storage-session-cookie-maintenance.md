# libSceNKWebKit

## add Functional

- [WebCore::NetworkStorageSession jar maintenance](../../../core/libs/prx/libSceNKWebKit/src/SessionCookieMaintenance.cpp) (libSceNKWebKit) - not compared with a console module. `getAllCookies` returns every unexpired cookie of the jar described in the cookie storage entry, in insertion order; `deleteAllCookies` clears that jar; `flushCookieStore` does nothing, as the host-memory jar has no deferred write to flush. The order is not one the console has been measured for. Closes when: compared with a console module.

## remove `_ZN7WebCore21NetworkStorageSession13getAllCookiesEv`
