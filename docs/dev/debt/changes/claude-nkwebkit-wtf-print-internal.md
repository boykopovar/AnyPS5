# libSceNKWebKit

## add Functional

- [WTF::printInternal](../../../core/libs/prx/libSceNKWebKit/src/WtfPrintStream.cpp) (libSceNKWebKit) - `PrintStream::printf`, the `const char*`, `CString`, `bool`, `int`, `unsigned`, `long`, `unsigned long`, `double` and `RawPointer` overloads and `dumpCharacter` follow the pinned [PrintStream.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/PrintStream.cpp#L38-L216) and only pass their format and value to the guest stream's `vprintf` with a SysV `va_list`; formatting is the stream's job. The vtable slots (destructor pair, `vprintf` 2, `flush` 3, `begin` 4, `end` 5) come from the declaration order in [PrintStream.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/PrintStream.h#L53-L101) and are not measured on a console. `long` and `unsigned long` are 64-bit as on the console, also on Windows hosts.

## add Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceNKWebKit/Export.cpp) - 5928 of the 8703 inventory declarations listed below throw through `NotImplemented_nid_no_patch`; signatures and behavior are unverified. The later 13.60-labelled image contains all 8703; 8448 match the public 3.20 catalogue, while 255 are absent and their presence in retail 3.20 is unverified. Per-image records show the latter in a separate 3.20-labelled image, but neither image label is authenticated. Identified names match NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv). Data exports need separate analysis.

## remove `_ZN3WTF11PrintStream6printfEPKcz`

## remove `_ZN3WTF13dumpCharacterERNS_11PrintStreamEc`

## remove `_ZN3WTF13printInternalERNS_11PrintStreamENS_10RawPointerE`

## remove `_ZN3WTF13printInternalERNS_11PrintStreamEPKc`

## remove `_ZN3WTF13printInternalERNS_11PrintStreamERKNS_7CStringE`

## remove `_ZN3WTF13printInternalERNS_11PrintStreamEb`

## remove `_ZN3WTF13printInternalERNS_11PrintStreamEd`

## remove `_ZN3WTF13printInternalERNS_11PrintStreamEi`

## remove `_ZN3WTF13printInternalERNS_11PrintStreamEj`

## remove `_ZN3WTF13printInternalERNS_11PrintStreamEl`

## remove `_ZN3WTF13printInternalERNS_11PrintStreamEm`
