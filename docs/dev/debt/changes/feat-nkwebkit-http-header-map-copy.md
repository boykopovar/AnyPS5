# libSceNKWebKit

## add Unknown function info

- [HTTPHeaderMap::isolatedCopy](../../../core/libs/prx/libSceNKWebKit/src/HttpHeaderMap.cpp) (libSceNKWebKit) - follows [WebKit's const overload](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/network/HTTPHeaderMap.cpp), which matches the export's qualifiers. The export uses the existing 32-byte map layout and string isolation helpers. Closes when: return ABI and copy behavior are measured on console.
