# libfontconfig

## add Callable exports

- [FcFontRenderPrepare](../../../core/libs/prx/libfontconfig/Objects.cpp) (libfontconfig, NID `CvqQqWqcO-0`) - not measured or disassembled on a console. It follows Fontconfig 2.15.0's [fcmatch.c](https://github.com/fontconfig/fontconfig/blob/72b9a48f57de6204d99ce1c217b5609ee92ece9b/src/fcmatch.c): each font object keeps the value its matcher scores best against the pattern, objects only one side has are copied, and family, style and full-name lists are reordered by the pattern's name language. Adapter patterns keep no value bindings and hold no ranges, so every value counts as strong and the variable-font `fontvariations` string is never built. If every pairing scores NaN (a NaN double, or 0/0 for two empty PostScript names), upstream stores an uninitialized value; here the font's first value is kept. The closing substitution calls this module's `FcConfigReference`, which returns null; a non-null reference throws, as does a null pattern or font, which 2.15.0 asserts against.

## remove | `FcFontRenderPrepare` | `CvqQqWqcO-0` |
