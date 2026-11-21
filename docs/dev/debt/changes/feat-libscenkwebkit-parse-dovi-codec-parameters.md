# libSceNKWebKit

## add Older callable exports

- [WebCore::parseDoViCodecParameters](../../../core/libs/prx/libSceNKWebKit/src/DoViCodecParameters.cpp) (NID `kPSQIkWwKtM`): the console ABI is unmeasured. The export takes `const WTF::String&`, the signature [HEVCUtilities.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/graphics/HEVCUtilities.h#L45-L51) had until WebKit d7549ecd9331 replaced it together with its result type, so this follows that revision: `Optional<DoViParameterSet>` through the hidden result pointer (24 bytes: flag at 0, `codecName` at 8, profile at 16, level at 18), codec and profile names matched exactly. Whitespace around a number is `isSpaceOrNewline`, its Bidi white-space characters above U+00FF listed by hand. `codecName` is a new string on each call where WebKit shares its table's. A non-empty String with null storage throws. Closes when measured on the console.
