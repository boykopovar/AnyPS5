# libSceNKWebKit

## add Functional

- [Inspector::ContentSearchUtilities::textPositionFromOffset](../../../core/libs/prx/libSceNKWebKit/src/InspectorContentSearchUtilities.cpp) (libSceNKWebKit) - not compared with a console module. It follows [ContentSearchUtilities.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531a/Source/JavaScriptCore/inspector/ContentSearchUtilities.cpp#L67-L76) with the `ReturnAdjacentElementIfKeyIsNotPresent` search of [StdLibExtras.h](https://github.com/WebKit/WebKit/blob/e12a212a531a/Source/WTF/wtf/StdLibExtras.h#L203-L240); the [PlayStation source](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/inspector/ContentSearchUtilities.cpp#L67-L76) is the same. An empty line-ending vector throws `std::out_of_range` where WebKit's `Vector::at` crashes. Closes when: compared with a console module.

## remove `_ZN9Inspector22ContentSearchUtilities22textPositionFromOffsetEmRKN3WTF6VectorImLm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE`
