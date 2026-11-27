# libSceNKWebKit

## add Functional

- [WebCore::prefetchDNS and WebCore::stopResolveDNS](../../../core/libs/prx/libSceNKWebKit/src/DNS.cpp) (libSceNKWebKit) - not compared with a console module. Both return without doing anything. They reach the network backend's DNS queue, and in the curl backend that the PlayStation port uses, [DNSResolveQueueCurl.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531a/Source/WebCore/platform/network/curl/DNSResolveQueueCurl.cpp) leaves `platformResolve` and `stopResolve` as `notImplemented()`, so a prefetch never resolves a name and there is never a pending resolve to stop. `resolveDNS`, whose completion handler the curl backend never calls, stays a throwing stub. Closes when: compared with a console module.

## remove `_ZN7WebCore11prefetchDNSERKN3WTF6StringE`

## remove `_ZN7WebCore14stopResolveDNSEm`
