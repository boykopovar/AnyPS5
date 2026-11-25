# libSceNKWebKit

## change [HEVC codec parameters]

- [HEVC codec parameters](../../../core/libs/prx/libSceNKWebKit/src/HEVCCodecParameters.cpp) - console ABI and behavior are unmeasured. The export takes `const WTF::String&`, the signature [HEVCUtilities.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/graphics/HEVCUtilities.h#L33-L43) had until WebKit d7549ecd9331 replaced it together with its result type, so this follows that revision: `Optional<HEVCParameterSet>` through the hidden result pointer (48 bytes: flag at 0, `codecName` at 8, the `Vector<unsigned short>` of six constraint flags at 32) and the compatibility flags as written, not bit-reversed. Whitespace around a number is `isSpaceOrNewline`, its Bidi white-space characters above U+00FF listed by hand. Null accessed storage throws instead of faulting. Closes when: measured on console.
