# libSceNKWebKit

## add Functional

- [JSPromise status, result and isHandled](../../../core/libs/prx/libSceNKWebKit/src/JSPromiseState.cpp) (libSceNKWebKit) - follow the pinned [JSPromise.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/JSPromise.cpp#L81-L105) and [JSCJSValueInlines.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/JSCJSValueInlines.h#L694-L700) `asUInt32AsAnyInt`. The internal fields (flags at 0x10, reactions or result at 0x18 of [JSPromiseAbi.hpp](../../../core/libs/prx/libSceNKWebKit/src/JSPromiseAbi.hpp)) and the JSValue number encoding were read from the module with SHA-256 `3af992253665387feeb5df90ff27dff683e3aa270cc4d92a0535b46ab18fa20d`. The `VM` argument is unused. The cell header and butterfly are not compared with a console module.

## remove | `_ZNK3JSC9JSPromise6resultERNS_2VME` | `Ctz7dKz3Da0` |

## remove | `_ZNK3JSC9JSPromise6statusERNS_2VME` | `u9H28omaVEc` |

## remove | `_ZNK3JSC9JSPromise9isHandledERNS_2VME` | `NgxfVG1k2Vg` |
