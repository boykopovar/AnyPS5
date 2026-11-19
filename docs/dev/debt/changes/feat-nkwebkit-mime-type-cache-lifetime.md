# libSceNKWebKit

## remove `_ZN7WebCore13MIMETypeCacheD0Ev`

## remove `_ZN7WebCore13MIMETypeCacheD2Ev`

## remove `_ZNK7WebCore13MIMETypeCache7isEmptyEv`

## change [Forwarded exports]

- [Forwarded exports](../../../core/libs/prx/libSceNKWebKit/src/ForwardExports.cpp) (libSceNKWebKit) - 275 exports are the same function under another name in the PS5 11.00 module: the body at their address is a `jmp rel32` into another export (26 distinct targets), implemented as an assembly tail jump. The jump keeps every register, so the return type and arguments are the target's: 202 reach an implemented export (104 delete-family to `WTF::fastFree`, 92 new-family to `WTF::fastMalloc`, 2 to `WKMutableArrayCreate`, 2 to a WebCore::Region constructor, 1 to `WebCore::MIMETypeRegistry::isSupportedImageMIMEType` and 1 to the `WebCore::MIMETypeCache` base destructor) and 73 reach an export that still throws, so they throw too; no prototype is claimed. From the 11.00 `libSceNKWebKit.sprx` (SHA-256 `3af992253665387feeb5df90ff27dff683e3aa270cc4d92a0535b46ab18fa20d`), each NID recomputed from the name; only 11.00 measured.

## add Functional

- [WebCore::MIMETypeCache destructors and isEmpty](../../../core/libs/prx/libSceNKWebKit/src/MimeTypeCache.cpp) (libSceNKWebKit) - not compared with a console module. The 0x28-byte layout assumes the module's `WTF::Optional` is the bool-first class of [Optional.h](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WTF/wtf/Optional.h#L311-L315) (exports still mangle `WTF::Optional`); the four byte stores of the module constructor also fit libc++ `std::optional`. Per [MIMETypeCache.h](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/graphics/MIMETypeCache.h#L61-L62), the base destructor (and the complete one that jumps to it) releases the cached-result and supported-type hash tables, and the deleting one frees the object with `WTF::fastFree`. `isEmpty` is true only for an engaged set with no keys. A null object throws. Closes when: compared with a console module.
