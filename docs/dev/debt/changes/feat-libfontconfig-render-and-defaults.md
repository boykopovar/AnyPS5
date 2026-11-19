# libfontconfig

## add Functional

- [FcDefaultSubstitute](../../../core/libs/prx/libfontconfig/Export.cpp) (NID `sqEJZ2yMVNw`) follows the public Fontconfig 2.15.0 [fcdefault.c](https://gitlab.freedesktop.org/fontconfig/fontconfig/-/blob/2.15.0/src/fcdefault.c), not console evidence; console behavior is unverified. Range-valued sizes are not averaged, because patterns cannot hold `FcTypeRange` values. With no PS5 locale environment, an absent `namelang` becomes `en`, as 2.15.0 does when `FC_LANG`, `LC_ALL`, `LC_CTYPE` and `LANG` are unset, and the `en-us` added to `familylang`, `stylelang` and `fullnamelang` has no weak binding. `prgname` and `desktop` are never added, as their console sources are unknown. An integer `pixelsize` is read as a double, and a null pattern throws.

## add Callable exports

- [FcFontRenderPrepare](../../../core/libs/prx/libfontconfig/Objects.cpp) (libfontconfig, NID `CvqQqWqcO-0`) - not measured or disassembled on a console. It follows Fontconfig 2.15.0's [fcmatch.c](https://github.com/fontconfig/fontconfig/blob/72b9a48f57de6204d99ce1c217b5609ee92ece9b/src/fcmatch.c): each font object keeps the value its matcher scores best against the pattern, objects only one side has are copied, and family, style and full-name lists are reordered by the pattern's name language. Adapter patterns keep no value bindings and hold no ranges, so every value counts as strong and the variable-font `fontvariations` string is never built. If every pairing scores NaN (a NaN double, or 0/0 for two empty PostScript names), upstream stores an uninitialized value; here the font's first value is kept. The closing substitution calls this module's `FcConfigReference`, which returns null; a non-null reference throws, as does a null pattern or font, which 2.15.0 asserts against.

## remove | `FcFontRenderPrepare` | `CvqQqWqcO-0` |

## remove | `FcDefaultSubstitute` | `sqEJZ2yMVNw` |
