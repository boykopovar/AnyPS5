# libSceJsc

## change [JSString conversion]

- [JSString conversion](../../../core/libs/prx/libSceJsc/Export.cpp): console behavior is unverified. UTF-16 storage and the null/empty distinction follow pinned WebKit [OpaqueJSString.cpp](https://github.com/WebKit/WebKit/blob/f274ef4c03ebace644c46f8ec55746b391acdcc8/Source/JavaScriptCore/API/OpaqueJSString.cpp). Malformed UTF-8 creates a null string, distinct from an empty string. `JSStringGetUTF8CString` follows the 2020 [JSStringRef.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/JavaScriptCore/API/JSStringRef.cpp#L99-L117), which the PS5 11.x tree still has: output stops before an unpaired UTF-16 surrogate, the buffer is terminated there and the function returns 0. The PS5 11.x [OpaqueJSString.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/API/OpaqueJSString.h#L49-L52) keeps the low 32 bits of a UTF-16 length of 2^32 or more, which throws here as in pinned WebKit.

# libSceNKWebKit

## add Functional

- [JS API text with an unpaired surrogate](../../../core/libs/prx/libSceNKWebKit/src/JscContext.cpp) (libSceNKWebKit) - a `JSStringRef` that the JS API hands to QuickJS as UTF-8 (script source and URL, the text of `JSObjectMakeFunction`, JSON text, symbol descriptions, private property names, the context name) is cut at its first unpaired UTF-16 surrogate, where JavaScriptCore keeps the code unit. Property names keep it. Closes when: these strings reach QuickJS as UTF-16.
