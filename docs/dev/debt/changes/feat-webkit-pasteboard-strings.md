# libSceNKWebKit

## add Functional

- [WebCore::Pasteboard string clipboard](../../../core/libs/prx/libSceNKWebKit/src/PasteboardStrings.cpp) (libSceNKWebKit) - not compared with a console module. Follows [PasteboardLibWPE.cpp:74-88](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/libwpe/PasteboardLibWPE.cpp#L74-L88) and the non-Cocoa `readAllStrings` of Pasteboard.cpp:51-58, but the platform pasteboard is not modelled: `PlatformPasteboard` and the `wpe_pasteboard_*` exports of libSceNKWebKitRequirements throw, so the strings live in one module store keyed by type and a write replaces its type. `readString` answers a null `String` for a type never written and for an empty value, as `PlatformPasteboard::readString` does for a zero-length string; `readAllStrings` answers no entries then and one otherwise. Null handles and null arguments throw `std::invalid_argument`. Closes when: compared with a console module.
