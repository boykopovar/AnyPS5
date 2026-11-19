# libSceNKWebKit

## change [WebCore::HTTPHeaderMap]

- [WebCore::HTTPHeaderMap](../../../core/libs/prx/libSceNKWebKit/src/HttpHeaderMap.cpp) - the console module layout is unverified. The 32-byte map, two 16-byte vectors, default-int enum numbered by sorted names, string ownership and release operations follow [the pinned headers and implementation](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/HTTPHeaderMap.h); vector allocation uses WTF fastMalloc/fastFree. Null objects, null accessed storage, overflowing vector/value sizes and name-table indices outside 0–94 throw. `isolatedCopy` has no ref qualifier, so it follows [WebKit 73a35b43](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/network/HTTPHeaderMap.cpp#L44-L50): each vector is reserved to exactly its size and filled with `String::isolatedCopy` keys and values.
