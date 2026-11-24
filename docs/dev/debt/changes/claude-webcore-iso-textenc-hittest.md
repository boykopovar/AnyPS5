# libSceNKWebKit

## add Functional

- [ISO box, default text encoding and hit-test rect statics](../../../core/libs/prx/libSceNKWebKit/src/WebCoreHitTestLocation.cpp) (libSceNKWebKit) - not compared with a console module: the device's WebKit revision is unverified. The 3.20 `ISOWebVTTCue::boxTypeName` (`vttc`), `ISOBox::minimumBoxSize` (8), `defaultTextEncodingNameForSystemLanguage` (non-Cocoa `ISO-8859-1`) and `HitTestLocation::rectForPoint` follow WebKit 73a35b43 of August 2020; `rectForPoint` floors as `LayoutUnit::floor` does and pads as [HitTestLocation.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/rendering/HitTestLocation.cpp#L150-L161) does, with 32-bit wrapping padding sums. Closes when: compared with a console module.
