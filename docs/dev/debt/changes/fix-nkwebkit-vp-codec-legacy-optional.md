# libSceNKWebKit

## change [VP codec parser]

- [VP codec parser](../../../core/libs/prx/libSceNKWebKit/src/VPCodec.cpp) (`nmGqNyrNQj0`) is not compared with a console module. The export is in the 3.20 inventory only and follows [VP9Utilities.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/graphics/VP9Utilities.cpp#L125-L232): it returns WTF's `Optional<VPCodecConfigurationRecord>` through the hidden pointer, engaged byte at 0 and the 16-byte record at 8, and accepts only `vp08` and `vp09`. WebKit accepts 2 (Unspecified) for the colour primaries, transfer characteristics and matrix coefficients since [2020-07-22](https://github.com/WebKit/WebKit/commit/14f437c7aa38) and refused it in the eight days before, both inside the window the 3.20 names allow, so a string that parses only with that value throws `std::logic_error`. Closes when: compared with a console module.

## change [VP codec decimal parsing]

- [VP codec decimal parsing](../../../core/libs/prx/libSceNKWebKit/src/VPCodec.cpp) follows [StringToIntegerConversion.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WTF/wtf/text/StringToIntegerConversion.h#L33-L122), unchanged across the 3.20 window. Whitespace is skipped on both sides as [isSpaceOrNewline](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WTF/wtf/text/StringImpl.h#L770-L774) defines it: the ASCII space and U+0009 to U+000D for Latin-1, and for other UTF-16 units the bidi class WS, written here as the fixed list U+1680, U+2000 to U+200A, U+2028, U+205F and U+3000 instead of a call to ICU. A leading plus is accepted, unsigned overflow and trailing junk rejected. Console execution is unverified.
