# libSceNKWebKit

## add Functional

- [WebCore::NetworkStorageSession::hasCookies](../../../core/libs/prx/libSceNKWebKit/src/NetworkStorageSessionCookies.cpp) (libSceNKWebKit) - not compared with a console module. It calls the completion handler once with `false` and releases it, as the curl backend the PlayStation port uses does in the pinned [NetworkStorageSessionCurl.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531a/Source/WebCore/platform/network/curl/NetworkStorageSessionCurl.cpp), where the function is a `FIXME` that answers `false` without reading the cookie database. A null handler throws. Closes when: compared with a console module.

## remove `_ZNK7WebCore21NetworkStorageSession10hasCookiesERKNS_17RegistrableDomainEON3WTF17CompletionHandlerIFvbEEE`
