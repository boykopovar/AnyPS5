# libSceNKWebKit

## add Unknown function info

- [ISO WebVTT cue object lifetime](../../../core/libs/prx/libSceNKWebKit/src/WebCoreIsoVttCueLifetime.cpp) (libSceNKWebKit) - the 3.20 console ABI and vtable are unmeasured. The 0x70-byte cue and its string move order follow [ISOVTTCue.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/graphics/iso/ISOVTTCue.cpp#L77-L92) and [ISOBox.h](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/graphics/iso/ISOBox.h); the vtable provides destruction and has no RTTI. The vtable's parse entry still throws. The public deleting destructor remains unimplemented. A null argument throws. Closes when: compared with a console module.

## remove | `_ZN7WebCore12ISOWebVTTCueD2Ev` | `66lUg3KSBQs` |
