# libSceNKWebKit

## add Functional

- [JSC::BytecodeCacheError::isValid](../../../core/libs/prx/libSceNKWebKit/src/JscBytecodeCacheError.cpp) (libSceNKWebKit) - not compared with a console module. It follows [BytecodeCacheError.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/JavaScriptCore/runtime/BytecodeCacheError.cpp#L69-L74) and [ParserError.h](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/JavaScriptCore/parser/ParserError.h#L81), kept by the [11.x fork](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/BytecodeCacheError.cpp#L72-L77): a ParserError (variant index 0) is valid when its `m_type` byte at offset 84 is not `ErrorNone`, and a StandardError or WriteError is always valid. The one-byte index sits at offset 88 after the 88-byte ParserError in both. A null error or a valueless or out-of-range index throws. Closes when: compared with a console module.

## remove `_ZNK3JSC18BytecodeCacheError7isValidEv`
