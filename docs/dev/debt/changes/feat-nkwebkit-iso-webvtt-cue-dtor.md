# libSceNKWebKit

## remove `_ZN7WebCore12ISOWebVTTCueD0Ev`

## remove `_ZN7WebCore12ISOWebVTTCueD2Ev`

## change [Forwarded exports]

- [Forwarded exports](../../../core/libs/prx/libSceNKWebKit/src/ForwardExports.cpp) (libSceNKWebKit) - 275 exports are the same function under another name in the PS5 11.00 module: the body at their address is a `jmp rel32` into another export (26 distinct targets), implemented as an assembly tail jump. The jump keeps every register, so the return type and arguments are the target's: 202 reach an implemented export (104 delete-family to `WTF::fastFree`, 92 new-family to `WTF::fastMalloc`, 2 to `WKMutableArrayCreate`, 2 to a WebCore::Region constructor, 1 to `WebCore::MIMETypeRegistry::isSupportedImageMIMEType` and 1 to the `WebCore::ISOWebVTTCue` base destructor) and 73 reach an export that still throws, so they throw too; no prototype is claimed. From the 11.00 `libSceNKWebKit.sprx` (SHA-256 `3af992253665387feeb5df90ff27dff683e3aa270cc4d92a0535b46ab18fa20d`), each NID recomputed from the name; only 11.00 measured.

## add Functional

- [WebCore::ISOWebVTTCue destructors](../../../core/libs/prx/libSceNKWebKit/src/WebCoreIsoVttCue.cpp) (libSceNKWebKit) - not compared with a console module. The base destructor, and the complete destructor that jumps to it, release the cue text, settings, original start time, identifier and source ID strings and then the `ISOBox` extended type vector, the implicit destructor of the 0x70-byte layout from [ISOVTTCue.h](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/graphics/iso/ISOVTTCue.h#L45-L139). The deleting destructor then frees the object with `ISOBox::operator delete` (`WTF::fastFree`). The vtable pointer is not reset to the `ISOBox` vtable, which no export exposes. A null object throws. Closes when: compared with a console module.
