# libSceNKWebKit

## add Functional

- [WTF::printInternal](../../../core/libs/prx/libSceNKWebKit/src/WtfPrintStream.cpp) (libSceNKWebKit) - `PrintStream::printf`, the `const char*`, `CString`, `bool`, `int`, `unsigned`, `long`, `unsigned long`, `double` and `RawPointer` overloads and `dumpCharacter` follow the pinned [PrintStream.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/PrintStream.cpp#L38-L216) and only pass their format and value to the guest stream's `vprintf` with a SysV `va_list`; formatting is the stream's job. The vtable slots (destructor pair, `vprintf` 2, `flush` 3, `begin` 4, `end` 5) come from the declaration order in [PrintStream.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/PrintStream.h#L53-L101) and are not measured on a console. `long` and `unsigned long` are 64-bit as on the console, also on Windows hosts.

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
