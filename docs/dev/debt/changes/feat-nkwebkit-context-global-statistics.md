# libSceNKWebKit

## remove | `WKContextGetGlobalStatistics` | `cLN+-CVotuU` |

## add Functional

- [WKContextGetGlobalStatistics](../../../core/libs/prx/libSceNKWebKit/src/ContextStatistics.cpp) (libSceNKWebKit) - writes the three `unsigned` fields of `WKContextStatistics` from [WKContextPrivate.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebKit/UIProcess/API/C/WKContextPrivate.h). The counts are the live host view, page and frame objects of this module. No export creates these objects yet, so a title currently reads zero. The three counts are loaded one by one, so a snapshot taken while objects are created or released can mix different instants. How the console updates its counters is unmeasured. An unwritable output throws. Closes when: compared with the console.
