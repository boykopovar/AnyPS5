# libSceNKWebKit

## add Functional

- [WTF::StringImpl::utf8ForCharacters](../../../core/libs/prx/libSceNKWebKit/src/WtfString.cpp) (libSceNKWebKit) - not compared with a console module. Both overloads reuse the conversion behind `StringImpl::utf8`, `String::utf8` and `StringView::utf8`: the 16-bit one takes the same `ConversionMode` (lenient writes U+FFFD for an unpaired surrogate, except a lead surrogate that ends the buffer, which it encodes as three bytes; strict throws `invalid_argument`; the replacing mode writes U+FFFD for every unpaired surrogate) and the 8-bit one reads Latin-1 and has no mode. A null buffer with a nonzero length throws; a zero length gives an empty `CString`. How the console reports a strict-mode failure is unmeasured.

- [WTF::FileSystemImpl::makeAllDirectories](../../../core/libs/prx/libSceNKWebKit/src/WtfSystem.cpp) (libSceNKWebKit) - source-guided by pinned [FileSystemPOSIX.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f97/Source/WTF/wtf/posix/FileSystemPOSIX.cpp#L332-L356), not compared with a console module; the device's WebKit revision is unverified. It creates each missing component through the guest `stat` and `mkdir` with mode 0700 and returns true when the path already exists, including a file. A null or empty `String` returns false where WebKit reads out of bounds. Whether the console applies the same mode and how it reports a blocked component is unmeasured.

## remove | `_ZN3WTF10StringImpl17utf8ForCharactersEPKDsjNS_14ConversionModeE` | `NvD7vqlu22E` |

## remove | `_ZN3WTF10StringImpl17utf8ForCharactersEPKhj` | `o5VwYgTgLtU` |

## remove | `_ZN3WTF14FileSystemImpl18makeAllDirectoriesERKNS_6StringE` | `yUU8KDyU5Xw` |
