# libSceNKWebKit

## add Functional

- [WebCore::Pasteboard plain text and factory](../../../core/libs/prx/libSceNKWebKit/src/PasteboardPlainText.cpp) (libSceNKWebKit) - not compared with a console module. `createForCopyAndPaste` returns `std::unique_ptr<Pasteboard>` through the indirect result pointer of the Pasteboard.h declaration, around a handle this module allocates per call; the console's `Pasteboard` layout and destructor are not modelled and none is exported, so a caller that frees it frees an object this module allocated. `writePlainText` follows [PasteboardLibWPE.cpp:150-153](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/libwpe/PasteboardLibWPE.cpp#L150-L153): it writes under `text/plain;charset=utf-8` and ignores the `SmartReplaceOption` this port never forwards. The console sends both through the platform pasteboard, unmodelled; null handles and null arguments throw `std::invalid_argument`. Closes when: compared with a console module.

## remove `_ZN7WebCore10Pasteboard14writePlainTextERKN3WTF6StringENS0_18SmartReplaceOptionE`
