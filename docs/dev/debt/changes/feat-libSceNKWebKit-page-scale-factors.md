# libSceNKWebKit

## add Functional

- [WKPage scale factors](../../../core/libs/prx/libSceNKWebKit/src/PageScaleFactors.cpp) (libSceNKWebKit) - `WKPageGetBackingScaleFactor`, `WKPageSetCustomBackingScaleFactor`, `WKPageGetScaleFactor` and `WKPageSetScaleFactor` read and write host `Page` state: the backing scale factor is the custom value, cleared by 0, or the intrinsic 1, and the page scale factor starts at 1. No export creates a `WKPage` or runs a web process for it yet, so nothing is sent to one and the scale origin is not used. `WKPageSetScaleFactor` stores any value, as a release build of WebKit does (it only asserts `scale > 0`). Both setters throw on a suspended page where WebKit crashes. Follows the pinned `WKPage.cpp` and `WebPageProxy.cpp`; not compared with a console module. Closes when: compared with a console module.

## remove `WKPageGetBackingScaleFactor` | `e0jjfsQdveM`

## remove `WKPageSetCustomBackingScaleFactor` | `iot3o+WKLVI`

## remove `WKPageGetScaleFactor` | `is2FV8Y-bMc`

## remove `WKPageSetScaleFactor` | `LDsNIDCwcLY`
