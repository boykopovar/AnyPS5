# libSceNKWebKit

## add Functional

- [WebCore::ResourceRequestBase 3.20 exports](../../../core/libs/prx/libSceNKWebKit/src/LegacyResourceRequestAbi.hpp) (libSceNKWebKit) - not compared with a console module. `setHTTPBody(RefPtr<FormData>&&)` and `httpHeaderField(const String&)` are in the 3.20 inventory only and use the 184-byte layout of [ResourceRequestBase.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/network/ResourceRequestBase.h#L235-L255), unchanged from 2020-06-13 to 2020-11-12, the window the 3.20 names allow. `httpHeaderField` still numbers common headers as the later revision does (95 names; `HTTPHeaderNames.in` had 84 then), like the rest of `HTTPHeaderMap`, so a common header that 3.20 code stored under its own number is not found. Closes when: compared with a console module.
