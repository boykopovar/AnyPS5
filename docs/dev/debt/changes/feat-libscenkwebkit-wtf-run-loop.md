# libSceNKWebKit

## change [WKRunLoop main run loop dispatch]

- [WKRunLoop main run loop dispatch](../../../core/libs/prx/libSceNKWebKit/src/RunLoop.cpp) (libSceNKWebKit) - `WKRunLoopInitializeMain` only runs `initializeMainThread`; the rest of `InitializeWebKit2` is unmodelled, and console execution is unverified. `WKRunLoopRunMain`, `WKRunLoopStopMain` and `WKRunLoopCallOnMainThread` run on `WTF::RunLoop::main()` as in the pinned [WKRunLoop.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebKit/UIProcess/API/C/playstation/WKRunLoop.cpp), so a stop while no loop runs is ignored; before the main thread is initialised they throw, except `WKRunLoopCallOnMainThread` with a null callback.

## change [WTF callOnMainThread and callOnMainRunLoop]

- [WTF callOnMainThread and callOnMainRunLoop](../../../core/libs/prx/libSceNKWebKit/src/WtfMainThread.cpp) (libSceNKWebKit) - console execution is unverified. The 8-byte `Function` and `[D1, D0, call]` vtable follow the pinned [Function.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/Function.h#L37-L121). A null or empty `Function` throws, and so does a call before `initializeMainThread`. A cross-thread `AndWait` rethrows the callable's exception after destroying it and blocks forever if the main loop never runs again. Otherwise follows [MainThread.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/MainThread.cpp#L66-L145) without `USE(WEB_THREAD)`, dispatching to `RunLoop::main()`. Closes when: compared with a console module.

## add Functional

- [WTF::RunLoop and RunLoop::TimerBase](../../../core/libs/prx/libSceNKWebKit/src/WtfRunLoop.cpp) (libSceNKWebKit) - layouts follow the generic event loop of the pinned [RunLoop.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/RunLoop.h) and are not checked against a console module. Only the `RunLoop` vtable pointer and reference count are guest-visible; its private members are host-side. `TimerBase` is 24 bytes and `DispatchTimer` 40. Timers use the guest monotonic clock. A null object, an empty `Function`, `main` before `initializeMainThread` and `fired` on a bare `TimerBase` throw. A `dispatchAfter` timer stopped before it fires leaks, as upstream. The libSceJsc `WTF::RunLoop` exports still throw. Otherwise follows [RunLoopGeneric.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/generic/RunLoopGeneric.cpp). Closes when: compared with a console module.

## remove | `_ZN3WTF7RunLoop13dispatchAfterENS_7SecondsEONS_8FunctionIFvvEEE` | `JkZwD6Lu-cg` |

## remove | `_ZN3WTF7RunLoop17setWakeUpCallbackEONS_8FunctionIFvvEEE` | `qIz+DBSTKZM` |

## remove | `_ZN3WTF7RunLoop38suspendFunctionDispatchForCurrentCycleEv` | `fKaloYAyEOM` |

## remove | `_ZN3WTF7RunLoop3runEv` | `c5zBiTJEZH8` |

## remove | `_ZN3WTF7RunLoop4mainEv` | `OOC9ufkAGWE` |

## remove | `_ZN3WTF7RunLoop4stopEv` | `Wg-SBhw2nm8` |

## remove | `_ZN3WTF7RunLoop5cycleEj` | `CKJOqWLNaIA` |

## remove | `_ZN3WTF7RunLoop7currentEv` | `jdKlGaUSjOw` |

## remove | `_ZN3WTF7RunLoop8dispatchEONS_8FunctionIFvvEEE` | `HOOe-g7LcDI` |

## remove | `_ZN3WTF7RunLoop9TimerBase4stopEv` | `ga3epfbYdYw` |

## remove | `_ZN3WTF7RunLoop9TimerBase5startENS_7SecondsEb` | `5ehgoYyaJmA` |

## remove | `_ZN3WTF7RunLoop9TimerBaseC2ERS0_` | `7fOcpBLva2Y` |

## remove | `_ZN3WTF7RunLoop9TimerBaseD1Ev` | `Fj3K7fq81Nw` |

## remove | `_ZN3WTF7RunLoop9TimerBaseD2Ev` | `CmEb6NZaH44` |

## remove | `_ZNK3WTF7RunLoop9TimerBase16secondsUntilFireEv` | `9ym46rnYKZA` |

## remove | `_ZNK3WTF7RunLoop9TimerBase8isActiveEv` | `uh7RQK-+n20` |
