# libSceJsc

## change [Older callable NIDs]

- [Older callable NIDs](../../../core/libs/prx/libSceJsc/LegacyExports.cpp): 196 exports have unverified signatures and behavior. 195 are named after the symbols the [verified NID name database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) gives for their NIDs, and the NID of each name matches; `NKE5vAmeA-o` has an unverified name. These are throwing import-resolution placeholders; no JavaScriptCore behavior is implemented. No matching NIDs were found in the later module inventory.

## add Functional

- [WTF UTF-8 conversion](../../../core/libs/prx/libSceJsc/src/WtfUtf8.cpp) - the console WTF and ICU versions and layouts are unverified. `CString` is one pointer to a `CStringBuffer` (4-byte refcount, length at 8, characters after it) and `Expected<CString, UTF8ConversionError>` adds a `bool` at offset 8, as in pinned [CString.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WTF/wtf/text/CString.h) and [Expected.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WTF/wtf/Expected.h). Guest code frees the buffers through libSceNKWebKit `fastFree`. The 11 exports port pinned UTF8Conversion.cpp, StringImpl.cpp and WTFString.cpp with WebKit's bundled ICU 62.2 macros. A full target, a range outside the string, a length above `INT_MAX`, a failed `String::utf8` and null arguments throw.

- [WTF JSON values](../../../core/libs/prx/libSceJsc/src/WtfJson.cpp) - the console WTF version and layouts are unverified; `asValue` and `asBoolean` are older NIDs. The six exports read a 24-byte `JSONImpl::Value` (vtable, refcount, type at 12, value at 16) and a 40-byte `ArrayBase` with its `Vector` at 24, as in pinned [JSONValues.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WTF/wtf/JSONValues.h). The last deref calls vtable slot 1. Null arguments throw. `escapeString`, the object and array builders and getters, `parseJSON` and `writeJSON` still throw: `StringBuilder` and `HashMap` are not modelled.

## remove | `_ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b` | `HblNi06Y6UI` |

## remove | `_ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb` | `UjTDE3AQCRg` |

## remove | `_ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_` | `AJ7+0UjKEww` |

## remove | `_ZN3WTF7Unicode43convertUTF8ToUTF16ReplacingInvalidSequencesEPKcS2_PPDsS3_Pb` | `PqZUVw-1dv0` |

## remove | `_ZNK3WTF10StringImpl10tryGetUtf8ENS_14ConversionModeE` | `cz5x0BcGOZ8` |

## remove | `_ZNK3WTF10StringImpl18tryGetUtf8ForRangeEjjNS_14ConversionModeE` | `PcudeTUiShQ` |

## remove | `_ZNK3WTF10StringView10tryGetUtf8ENS_14ConversionModeE` | `T-uddfL1AmI` |

## remove | `_ZNK3WTF6String10tryGetUtf8ENS_14ConversionModeE` | `iQoJc7qieSk` |

## remove | `_ZNK3WTF6String10tryGetUtf8Ev` | `87OOTOeB8pE` |

## remove | `_ZNK3WTF6String4utf8Ev` | `s+42ELWxjCI` |
