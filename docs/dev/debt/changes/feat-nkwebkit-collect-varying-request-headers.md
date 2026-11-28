# libSceNKWebKit

## add Functional

- [WebCore::collectVaryingRequestHeaders(const CookieJar*, ...)](../../../core/libs/prx/libSceNKWebKit/src/VaryingRequestHeaders.cpp) (libSceNKWebKit) - not compared with a console module. Only in the 3.20 inventory; follows the 2020 [CacheValidation.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/network/CacheValidation.cpp). For `Vary: Cookie` it calls the caller's `CookieJar` through slot 3 of its virtual table, with the two `Optional` identifiers passed by address and the `std::pair` result returned through a hidden pointer; slot and convention are derived from the pinned [CookieJar.h](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/loader/CookieJar.h) and `Optional.h`, not measured. The request uses the 3.20 layout, the response the module's accessors. Closes when: compared with a console module.
