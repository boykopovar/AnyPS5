# libSceNKWebKit

## remove `_ZNK9Inspector15ScriptCallFrame7isEqualERKS0_`

## change [Forwarded exports]

- [Forwarded exports](../../../core/libs/prx/libSceNKWebKit/src/ForwardExports.cpp) (libSceNKWebKit) - 275 exports are the same function under another name in the PS5 11.00 module: the body at their address is a `jmp rel32` into another export (26 distinct targets), implemented as an assembly tail jump. The jump keeps every register, so the return type and arguments are the target's: 202 reach an implemented export (104 delete-family to `WTF::fastFree`, 92 new-family to `WTF::fastMalloc`, 2 to `WKMutableArrayCreate`, 2 to a WebCore::Region constructor, 1 to `WebCore::MIMETypeRegistry::isSupportedImageMIMEType` and 1 to `Inspector::ScriptCallFrame::isEqual`) and 73 reach an export that still throws, so they throw too; no prototype is claimed. From the 11.00 `libSceNKWebKit.sprx` (SHA-256 `3af992253665387feeb5df90ff27dff683e3aa270cc4d92a0535b46ab18fa20d`), each NID recomputed from the name; only 11.00 measured.

## add Functional

- [Inspector::ScriptCallFrame::isEqual](../../../core/libs/prx/libSceNKWebKit/src/InspectorScriptCallFrame.cpp) (libSceNKWebKit) - not compared with a console module. `isEqual`, and `operator==`, which jumps to it, compare the function name, script name, pre-redirect URL, line and column and ignore the source ID, as the pinned [ScriptCallFrame.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/inspector/ScriptCallFrame.cpp#L60-L68) does. Strings compare by content, so 8-bit and 16-bit copies of the same text are equal and a null string differs from an empty one. A null frame throws, where the console would read through it. Closes when: compared with a console module.
