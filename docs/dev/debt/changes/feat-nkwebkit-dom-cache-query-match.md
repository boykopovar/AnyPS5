# libSceNKWebKit

## add Functional

- [DOMCacheEngine::queryCacheMatch](../../../core/libs/prx/libSceNKWebKit/src/DOMCacheEngine.cpp) (libSceNKWebKit) - both overloads, not compared with a console module. The one taking a cached request and response is in the 11.x inventory and reads 11.x requests; the one taking a URL and a map of stored headers is only in the 3.20 inventory and reads a 3.20 request. Both follow `DOMCacheEngine.cpp`, whose logic is the same in the two sources. The URL work goes through the `URL` exports of this library, and a request whose platform data is not up to date throws `std::logic_error`, as the other request exports do. Null arguments throw `std::invalid_argument` where the source dereferences them. Closes when: compared with a console module.

## remove `_ZN7WebCore14DOMCacheEngine15queryCacheMatchERKNS_15ResourceRequestES3_RKNS_16ResourceResponseERKNS_17CacheQueryOptionsE`
