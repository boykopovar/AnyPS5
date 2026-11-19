# libSceNKWebKit

## add Functional

- [WebCore ad click attribution string](../../../core/libs/prx/libSceNKWebKit/src/AdClickAttribution.cpp) (libSceNKWebKit) - not compared with a console module: the device's WebKit revision is unverified. `AdClickAttribution::toString` follows [AdClickAttribution.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/loader/AdClickAttribution.cpp#L212-L240) of August 2020. The earliest send time is reported only as within or outside 24 to 48 hours from `WallTime::now`, so the text changes with the clock. The result is 16-bit only where that revision's StringBuilder upconverts, and it is always a fresh string, not a substring of a builder buffer. Closes when: compared with a console module.
