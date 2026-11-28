# libSceNKWebKit

## add Functional

- [WTF::StringImpl::utf8ForCharacters](../../../core/libs/prx/libSceNKWebKit/src/WtfString.cpp) (libSceNKWebKit) - not compared with a console module. Both overloads return `Expected<CString, UTF8ConversionError>`, as [StringImpl.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WTF/wtf/text/StringImpl.h#L303-L304) declares them: the `CString` or the error first, the `has` flag at offset 8. They reuse the conversion behind `StringImpl::utf8`: the 16-bit one takes the same `ConversionMode` (lenient writes U+FFFD for an unpaired surrogate, except a lead surrogate that ends the buffer, which it encodes as three bytes; strict returns `IllegalSource`, or `SourceExhausted` for that final lead surrogate; the replacing mode writes U+FFFD for every unpaired surrogate) and the 8-bit one reads Latin-1 and has no mode. Over `MaxLength / 3` characters return `OutOfMemory`. A null buffer with a nonzero length throws; a zero length gives an empty `CString`.

- [WTF::FileSystemImpl::makeAllDirectories](../../../core/libs/prx/libSceNKWebKit/src/WtfSystem.cpp) (libSceNKWebKit) - source-guided by pinned [FileSystemPOSIX.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f97/Source/WTF/wtf/posix/FileSystemPOSIX.cpp#L332-L356), not compared with a console module; the device's WebKit revision is unverified. It creates each missing component through the guest `stat` and `mkdir` with mode 0700 and returns true when the path already exists, including a file. A null or empty `String` returns false where WebKit reads out of bounds. Whether the console applies the same mode and how it reports a blocked component is unmeasured.

## remove | `_ZN3WTF10StringImpl17utf8ForCharactersEPKDsjNS_14ConversionModeE` | `NvD7vqlu22E` |

## remove | `_ZN3WTF10StringImpl17utf8ForCharactersEPKhj` | `o5VwYgTgLtU` |

## remove | `_ZN3WTF14FileSystemImpl18makeAllDirectoriesERKNS_6StringE` | `yUU8KDyU5Xw` |
