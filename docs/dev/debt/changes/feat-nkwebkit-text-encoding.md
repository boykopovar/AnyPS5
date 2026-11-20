# libSceNKWebKit

## add Unknown function info

- [WebCore::TextEncoding](../../../core/libs/prx/libSceNKWebKit/src/TextEncoding.cpp) (libSceNKWebKit) - not compared with a console module. The 24-byte object and `Vector<uint8_t>` results follow the pinned `PAL::TextEncoding`; the vtable follows URL.h `URLTextEncoding` order (`encodeForURLParsing`, complete, deleting; sized `operator delete`) and the null typeinfo slot faults guest RTTI. The two-parameter `encode` export applies the pinned `NFCNormalize::Yes` default. Decode and encode throw `NotImplemented` for the [TextCodecICU.cpp:76–135](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/PAL/pal/text/TextCodecICU.cpp#L76-L135) encodings (ISO-8859-2/4/5/10/13-16, KOI8-R, macintosh, windows-1250/1251/1254/1256/1258, x-mac-*, EUC-TW): needs unimplemented libSceIcu ucnv exports. JIS/Big5/EUC-KR/GB18030 use the pinned reference arrays upstream asserts equal to its ICU tables.
