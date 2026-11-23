# libSceNKWebKit

## add Functional

- [WebCore::PlatformTimeRanges::dump](../../../core/libs/prx/libSceNKWebKit/src/PlatformTimeRangesDump.cpp) (libSceNKWebKit) - not compared with a console module; the 16-byte `PlatformTimeRanges` (`Vector<Range>` buffer, `unsigned` capacity, `unsigned` size) and 32-byte `Range` (two 16-byte `MediaTime`s) layouts follow the existing `MediaTypes.hpp` model and the pinned [PlatformTimeRanges.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/graphics/PlatformTimeRanges.h#L81-L132) and are unverified. Each range is one `print` of `[`, the start `MediaTime::dump`, `..`, the end `MediaTime::dump` and `] `, with nothing printed for no ranges; a null `this` or stream, a size above the capacity or a missing buffer throws `std::invalid_argument` before anything is printed instead of crashing. Closes when: compared with a console module.

## remove | `_ZNK7WebCore18PlatformTimeRanges4dumpERN3WTF11PrintStreamE` | `PeTaOgXASCo` |
