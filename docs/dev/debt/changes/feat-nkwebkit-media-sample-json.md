# libSceNKWebKit

## add Unknown function info

- [Media sample flags and JSON](../../../core/libs/prx/libSceNKWebKit/src/MediaSample.cpp) have no console ABI or execution verification. The checked guest vtable slots, flag bits and JSON member order follow Sony's PS5 11.x [MediaSample.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/MediaSample.h) and [MediaTime.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/MediaTime.cpp). The adapter writes those fixed JSON schemas directly using the existing WTF number formatter. Closes when: compared with a console module.

## change [Media and playback engine stubs]

- [Media and playback engine stubs](../../../core/libs/prx/libSceNKWebKit/src/Media.cpp) (libSceNKWebKit) - WebCore::MediaPlayer, HTMLMediaElement::fastSeek, TextTrackCueGeneric::create, MediaTime::toJSONObject, MediaTime::toString, MediaTimeRange::toJSONString, and dump(PrintStream&) exports remain throwing stubs in Export.cpp because they require the WebCore playback pipeline, DOM/engine objects, JSON object model, or PrintStream runtime.

## remove | `_ZNK3WTF9MediaTime12toJSONStringEv` | `D-8cZ6N7n4g` |
