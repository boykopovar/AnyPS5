# libSceNKWebKit

## add Functional

- [WebCore::PlatformTimeRanges::dump](../../../core/libs/prx/libSceNKWebKit/src/PlatformTimeRangesDump.cpp) (libSceNKWebKit) - not measured on a console. Adapted from PS5 11.x [PlatformTimeRanges.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/graphics/PlatformTimeRanges.cpp#L328-L335), the same in WebKit 73a35b43: each range is printed as `[`, start, `..`, end, `] ` inside one `begin`/`end` pair through the guest `PrintStream` vtable slots of the [WTF time dumps](../../../core/libs/prx/libSceNKWebKit/src/WtfTimeDump.cpp), and each `MediaTime` goes through the `MediaTime::dump` export on the begun stream. An empty set prints nothing. Null pointers, or a nonzero size with a null buffer, throw where WebKit faults.

## remove | `_ZNK7WebCore18PlatformTimeRanges4dumpERN3WTF11PrintStreamE` | `PeTaOgXASCo` |
