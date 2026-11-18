# libSceNKWebKit

## add Functional

- [Inspector backend dispatcher handler constructors](../../../core/libs/prx/libSceNKWebKit/src/InspectorBackendDispatcherHandler.cpp) (libSceNKWebKit) - not compared with a console module: the 22 handler vtables are this library's own, one `__cxa_pure_virtual` slot per command of the PS5 11.x [protocol](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/inspector/protocol) under the [PlayStation options](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/cmake/OptionsPlayStation.cmake#L215-L267), then the destructors. The slots are only reached while the base is constructed or destroyed. `D0` frees through the global `operator delete`.

## remove | `_ZN9Inspector27CSSBackendDispatcherHandlerC2ERKS0_` | `EgHudGJawMs` |

## remove | `_ZN9Inspector27CSSBackendDispatcherHandlerC2Ev` | `CEQjJzzBB+M` |

## remove | `_ZN9Inspector27CSSBackendDispatcherHandlerD0Ev` | `k801tcAVuJg` |

## remove | `_ZN9Inspector27DOMBackendDispatcherHandlerC2ERKS0_` | `6ZXtFYaM5Os` |

## remove | `_ZN9Inspector27DOMBackendDispatcherHandlerC2Ev` | `I+pR0r79q1A` |

## remove | `_ZN9Inspector27DOMBackendDispatcherHandlerD0Ev` | `Nj40sbvN+pk` |

## remove | `_ZN9Inspector28HeapBackendDispatcherHandlerC2ERKS0_` | `IDYAnLE0+kM` |

## remove | `_ZN9Inspector28HeapBackendDispatcherHandlerC2Ev` | `tai-HSaPBZU` |

## remove | `_ZN9Inspector28HeapBackendDispatcherHandlerD0Ev` | `k4HfprWhc0Q` |

## remove | `_ZN9Inspector28PageBackendDispatcherHandlerC2ERKS0_` | `Eh7uskcjFl4` |

## remove | `_ZN9Inspector28PageBackendDispatcherHandlerC2Ev` | `xAE5zO3kEi0` |

## remove | `_ZN9Inspector28PageBackendDispatcherHandlerD0Ev` | `AEnBqqmNJBA` |

## remove | `_ZN9Inspector29AuditBackendDispatcherHandlerC2ERKS0_` | `SZRzgfkPiGA` |

## remove | `_ZN9Inspector29AuditBackendDispatcherHandlerC2Ev` | `4hsvw3h04JU` |

## remove | `_ZN9Inspector29AuditBackendDispatcherHandlerD0Ev` | `m+Ju06sMg5k` |

## remove | `_ZN9Inspector30CanvasBackendDispatcherHandlerC2ERKS0_` | `60+np84nu94` |

## remove | `_ZN9Inspector30CanvasBackendDispatcherHandlerC2Ev` | `eNLB1x1rwMk` |

## remove | `_ZN9Inspector30CanvasBackendDispatcherHandlerD0Ev` | `Rcqpep1AOTM` |

## remove | `_ZN9Inspector30TargetBackendDispatcherHandlerC2ERKS0_` | `9RYsSzqyNrM` |

## remove | `_ZN9Inspector30TargetBackendDispatcherHandlerC2Ev` | `vfXUizQrSRo` |

## remove | `_ZN9Inspector30TargetBackendDispatcherHandlerD0Ev` | `qjHCYVd2NH0` |

## remove | `_ZN9Inspector30WorkerBackendDispatcherHandlerC2ERKS0_` | `3fxY0RvS5h0` |

## remove | `_ZN9Inspector30WorkerBackendDispatcherHandlerC2Ev` | `JZZchPyTk2k` |

## remove | `_ZN9Inspector30WorkerBackendDispatcherHandlerD0Ev` | `MPv4gGj4a4I` |

## remove | `_ZN9Inspector31BrowserBackendDispatcherHandlerC2ERKS0_` | `Uj4E1Axicy4` |

## remove | `_ZN9Inspector31BrowserBackendDispatcherHandlerC2Ev` | `bAc0bWqJiE0` |

## remove | `_ZN9Inspector31BrowserBackendDispatcherHandlerD0Ev` | `w4tKwcFJp-c` |

## remove | `_ZN9Inspector31ConsoleBackendDispatcherHandlerC2ERKS0_` | `F3ILcSPTa-c` |

## remove | `_ZN9Inspector31ConsoleBackendDispatcherHandlerC2Ev` | `iBQ6lAQw5Hw` |

## remove | `_ZN9Inspector31ConsoleBackendDispatcherHandlerD0Ev` | `cEcdsDQcKXU` |

## remove | `_ZN9Inspector31NetworkBackendDispatcherHandlerC2ERKS0_` | `wFpxQmjDScM` |

## remove | `_ZN9Inspector31NetworkBackendDispatcherHandlerC2Ev` | `zm2i2w7R9tU` |

## remove | `_ZN9Inspector31NetworkBackendDispatcherHandlerD0Ev` | `6eATaUH-V4Q` |

## remove | `_ZN9Inspector31RuntimeBackendDispatcherHandlerC2ERKS0_` | `ZVwY7ywc-0U` |

## remove | `_ZN9Inspector31RuntimeBackendDispatcherHandlerC2Ev` | `fK+DFgxPnbY` |

## remove | `_ZN9Inspector31RuntimeBackendDispatcherHandlerD0Ev` | `P01hbLcSwL8` |

## remove | `_ZN9Inspector32DatabaseBackendDispatcherHandlerC2ERKS0_` | `B1a1QgRyfug` |

## remove | `_ZN9Inspector32DatabaseBackendDispatcherHandlerC2Ev` | `PL2LcQggy9Q` |

## remove | `_ZN9Inspector32DatabaseBackendDispatcherHandlerD0Ev` | `8XCozccg+jU` |

## remove | `_ZN9Inspector32DebuggerBackendDispatcherHandlerC2ERKS0_` | `JhsJWxsvkZg` |

## remove | `_ZN9Inspector32DebuggerBackendDispatcherHandlerC2Ev` | `tGIL+vkTlJA` |

## remove | `_ZN9Inspector32DebuggerBackendDispatcherHandlerD0Ev` | `TI5ES3rnuTI` |

## remove | `_ZN9Inspector32TimelineBackendDispatcherHandlerC2ERKS0_` | `kFw-B-SsQUM` |

## remove | `_ZN9Inspector33AnimationBackendDispatcherHandlerC2ERKS0_` | `ueohGT35aPU` |

## remove | `_ZN9Inspector33AnimationBackendDispatcherHandlerC2Ev` | `Az0wDZ2JLeY` |

## remove | `_ZN9Inspector33AnimationBackendDispatcherHandlerD0Ev` | `sHIZ2KSx1C4` |

## remove | `_ZN9Inspector33InspectorBackendDispatcherHandlerC2ERKS0_` | `RSa71yEfRqU` |

## remove | `_ZN9Inspector33InspectorBackendDispatcherHandlerC2Ev` | `psb-WHk17uM` |

## remove | `_ZN9Inspector33InspectorBackendDispatcherHandlerD0Ev` | `REOBGOfo4WE` |

## remove | `_ZN9Inspector33LayerTreeBackendDispatcherHandlerC2ERKS0_` | `p+K1iHfbEbY` |

## remove | `_ZN9Inspector33LayerTreeBackendDispatcherHandlerC2Ev` | `xnIArSSzecM` |

## remove | `_ZN9Inspector33LayerTreeBackendDispatcherHandlerD0Ev` | `ClO-QNFpK2k` |

## remove | `_ZN9Inspector34DOMStorageBackendDispatcherHandlerC2ERKS0_` | `ti05wqo4hwA` |

## remove | `_ZN9Inspector34DOMStorageBackendDispatcherHandlerC2Ev` | `8Y6RLatuTeM` |

## remove | `_ZN9Inspector34DOMStorageBackendDispatcherHandlerD0Ev` | `kfT6hC5SF+g` |

## remove | `_ZN9Inspector35DOMDebuggerBackendDispatcherHandlerC2ERKS0_` | `NlMX-au+A9M` |

## remove | `_ZN9Inspector35DOMDebuggerBackendDispatcherHandlerC2Ev` | `54Tc57Y9FUA` |

## remove | `_ZN9Inspector35DOMDebuggerBackendDispatcherHandlerD0Ev` | `VF5HDDm3ZMk` |

## remove | `_ZN9Inspector38ScriptProfilerBackendDispatcherHandlerC2ERKS0_` | `HK0XarL0FJ0` |

## remove | `_ZN9Inspector38ScriptProfilerBackendDispatcherHandlerC2Ev` | `4dhx-ioq00U` |

## remove | `_ZN9Inspector38ScriptProfilerBackendDispatcherHandlerD0Ev` | `yaBlj3xh9TA` |

## remove | `_ZN9Inspector40ApplicationCacheBackendDispatcherHandlerC2ERKS0_` | `jHgwTMWTHPc` |

## remove | `_ZN9Inspector40ApplicationCacheBackendDispatcherHandlerC2Ev` | `YUU0Q9XyjqE` |

## remove | `_ZN9Inspector40ApplicationCacheBackendDispatcherHandlerD0Ev` | `pbs5-GMRRKY` |
