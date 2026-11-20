# libSceNKWebKit

## add Functional

- [setViewportFeature](../../../core/libs/prx/libSceNKWebKit/src/WebCoreViewport.cpp) (libSceNKWebKit) - not compared with a console module. A value with no numeric prefix, or an unknown `viewport-fit` keyword, is reported but leaves the argument and `widthWasExplicit` unchanged, where WebKit stores `0` or `auto`. A null error handler skips the report instead of faulting. Otherwise it follows the pinned [ViewportArguments.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531aeddc11259e8515671cf5188e038d/Source/WebCore/dom/ViewportArguments.cpp#L292-L481), whose `viewportFitEnabled` flag matches the exported name; the PS5 11.x source drops that flag. `minimal-ui` is reported as an unknown key, and messages are 8-bit whenever they fit in Latin-1. Closes when: compared with a console module.
