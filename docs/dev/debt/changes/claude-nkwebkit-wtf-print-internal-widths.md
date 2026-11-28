# libSceNKWebKit

## add Functional

- [WTF::printInternal](../../../core/libs/prx/libSceNKWebKit/src/WtfPrintStream.cpp) (libSceNKWebKit) - the vtable slots (`vprintf` 2, `flush` 3, `begin` 4, `end` 5) come from the declaration order in [PrintStream.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/PrintStream.h#L53-L101) and are not measured on a console. `PrintStream::printf` and the scalar, string, `RawPointer` and `dumpCharacter` overloads pass the format and promoted value of the pinned [PrintStream.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/PrintStream.cpp#L38-L216) to the guest `vprintf` with a SysV `va_list`; `long` is 64-bit, also on Windows hosts. `ClockType` follows [ClockType.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/ClockType.cpp#L33-L47) and throws for a value outside the enum.

## remove `_ZN3WTF11PrintStream6printfEPKcz`

## remove `_ZN3WTF13dumpCharacterERNS_11PrintStreamEc`

## remove `_ZN3WTF13printInternalERNS_11PrintStreamENS_10RawPointerE`

## remove `_ZN3WTF13printInternalERNS_11PrintStreamENS_9ClockTypeE`

## remove `_ZN3WTF13printInternalERNS_11PrintStreamEPKc`

## remove `_ZN3WTF13printInternalERNS_11PrintStreamERKNS_7CStringE`

## remove `_ZN3WTF13printInternalERNS_11PrintStreamEa`

## remove `_ZN3WTF13printInternalERNS_11PrintStreamEb`

## remove `_ZN3WTF13printInternalERNS_11PrintStreamEd`

## remove `_ZN3WTF13printInternalERNS_11PrintStreamEf`

## remove `_ZN3WTF13printInternalERNS_11PrintStreamEh`

## remove `_ZN3WTF13printInternalERNS_11PrintStreamEi`

## remove `_ZN3WTF13printInternalERNS_11PrintStreamEj`

## remove `_ZN3WTF13printInternalERNS_11PrintStreamEl`

## remove `_ZN3WTF13printInternalERNS_11PrintStreamEm`

## remove `_ZN3WTF13printInternalERNS_11PrintStreamEs`

## remove `_ZN3WTF13printInternalERNS_11PrintStreamEt`

## remove `_ZN3WTF13printInternalERNS_11PrintStreamEx`

## remove `_ZN3WTF13printInternalERNS_11PrintStreamEy`
