# libSceNKWebKit

## add Functional

- [WKContext process settings](../../../core/libs/prx/libSceNKWebKit/src/ContextProcessSettings.cpp) (libSceNKWebKit) - `WKContextSetPrewarmsProcessesAutomatically` and `WKContextSetFontAllowList` write host `ProcessPool` state: the prewarm flag on the pool's own configuration, and the font allow list, where a null array clears the list and non-string elements are skipped. `WKContextCreateWithConfiguration` now gives the context a copy of the configuration, as WebKit does, so the setter does not change the caller's configuration. No export runs a web process yet, so nothing is sent to one, and no query reads the font allow list back. Follows the pinned `WKContext.cpp` and `WebProcessPool.cpp`; not compared with a console module. Closes when: compared with a console module.

## remove `WKContextSetPrewarmsProcessesAutomatically` | `9LzZhiXxOL8`

## remove `WKContextSetFontAllowList` | `MhG9BMS7pSQ`
