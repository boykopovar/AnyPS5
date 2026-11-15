# libSceNKWebKit

## add Functional

- [WTF::StringImpl::utf8ForCharacters](../../../core/libs/prx/libSceNKWebKit/src/WtfString.cpp) (libSceNKWebKit) - not compared with a console module. Both overloads reuse the conversion behind `StringImpl::utf8`, `String::utf8` and `StringView::utf8`: the 16-bit one takes the same `ConversionMode` (lenient encodes an unpaired surrogate as three bytes, strict throws `invalid_argument`, the replacing mode writes U+FFFD) and the 8-bit one reads Latin-1 and has no mode. A null buffer with a nonzero length throws; a zero length gives an empty `CString`. How the console reports a strict-mode failure is unmeasured.

## remove | `_ZN3WTF10StringImpl17utf8ForCharactersEPKDsjNS_14ConversionModeE` | `NvD7vqlu22E` |

## remove | `_ZN3WTF10StringImpl17utf8ForCharactersEPKhj` | `o5VwYgTgLtU` |
