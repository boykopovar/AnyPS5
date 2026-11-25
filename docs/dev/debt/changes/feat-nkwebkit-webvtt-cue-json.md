# libSceNKWebKit

## add Unknown function info

- [ISO WebVTT cue JSON](../../../core/libs/prx/libSceNKWebKit/src/WebCoreIsoVttCue.cpp) (libSceNKWebKit) - console behavior and layout are unmeasured. Serializes the existing 0x70-byte cue layout with the keys and escapes in the pinned [ISOVTTCue.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/graphics/iso/ISOVTTCue.cpp#L126-L141). That PS5 11.x source emits `text` unconditionally. [Earlier WebKit](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/graphics/iso/ISOVTTCue.cpp#L118-L135) includes it only when logging is enabled. Which output the console module uses is unverified. A null cue throws. Closes when: compared with a console module.
