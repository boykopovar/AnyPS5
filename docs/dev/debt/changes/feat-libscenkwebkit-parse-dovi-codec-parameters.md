# libSceNKWebKit

## add Older callable exports

- [WebCore::parseDoViCodecParameters](../../../core/libs/prx/libSceNKWebKit/src/DoViCodecParameters.cpp) (NID `kPSQIkWwKtM`): the console ABI is unmeasured. The export takes `const WTF::String&`, but [HEVCUtilities.cpp L291–406](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/graphics/HEVCUtilities.cpp#L291-L406), which it follows, takes a StringView. The 8-byte `DoViParameters` and 12-byte optional (engaged byte 8, returned in RAX/RDX) come from the source and host tests. Lookups fold case as `PackedLettersLiteral` in SortedArrayMap.h does. A non-empty String with null storage throws. Closes when measured on the console.
