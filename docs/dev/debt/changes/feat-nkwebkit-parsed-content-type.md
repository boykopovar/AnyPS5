# libSceNKWebKit

## add Unknown function info

- [WebCore::ParsedContentType](../../../core/libs/prx/libSceNKWebKit/src/ParsedContentType.cpp) (libSceNKWebKit) - not compared with a console module. Non-Latin-1 whitespace in `Rfc2045` mode is a fixed list of the Unicode bidi WS code points, not ICU's `u_charDirection`. The 40-byte object, the 48-byte `Optional` with its flag first and the `HashMap<String, String>` table with metadata before the buckets follow [ParsedContentType.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/network/ParsedContentType.h), [Optional.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WTF/wtf/Optional.h#L312-L315) and [HashTable.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WTF/wtf/HashTable.h#L577-L592) at that revision.

## remove `_ZNK7WebCore17ParsedContentType9serializeEv`
