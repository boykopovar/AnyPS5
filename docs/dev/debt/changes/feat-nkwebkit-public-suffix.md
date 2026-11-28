# libSceNKWebKit

## add Functional

- [WebCore::isPublicSuffix](../../../core/libs/prx/libSceNKWebKit/src/PublicSuffix.cpp) - The console's public-suffix dataset and libpsl revision are unknown. The string-reference export follows [WebKit's curl port](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/network/curl/PublicSuffixCurl.cpp#L34-L42): locale-independent Unicode lowercase, lenient UTF-8, and both ICANN and private rules with the implicit star rule disabled. It uses the existing bundled libpsl 0.21.5 and pinned list, so suffix classifications can differ from console data. The existing 8-byte WTF String and StringImpl ownership model, ICU case mapping and UTF-8 helpers are reused. Invalid object pointers remain subject to their existing limitations; a null outer String throws instead of faulting. Console ABI and results are unmeasured.
