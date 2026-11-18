# libSceNKWebKit

## add Functional

- [WTF::Mutex and WTF::ThreadCondition](../../../core/libs/prx/libSceNKWebKit/src/WtfThreading.cpp) (libSceNKWebKit) - not compared with a console module. Failing pthread calls and a null object or NaN deadline throw where the source only asserts; `~ThreadCondition` ignores the result, as the source does. Each object is one 8-byte guest pthread handle, as in [ThreadingPrimitives.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/ThreadingPrimitives.h#L73-L119), and the calls follow the `PLATFORM(PLAYSTATION)` and pthread paths of [ThreadingPOSIX.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/posix/ThreadingPOSIX.cpp#L572-L669): a `PTHREAD_MUTEX_NORMAL` mutex named `SceNKWTF`, and a null default condition that guest libkernel initializes on first use. Closes when: measured in a title process.

## remove | `_ZN3WTF15ThreadCondition4waitERNS_5MutexE` | `Gabnudg8bzM` |

## remove | `_ZN3WTF15ThreadCondition6signalEv` | `FNJN5HdvU90` |

## remove | `_ZN3WTF15ThreadCondition9broadcastEv` | `cmb3wIXQ-Js` |

## remove | `_ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE` | `iulVS7DGVeg` |

## remove | `_ZN3WTF15ThreadConditionD1Ev` | `jfeZA8a0GbA` |

## remove | `_ZN3WTF15ThreadConditionD2Ev` | `FD7Kfj4cCDs` |

## remove | `_ZN3WTF5Mutex4lockEv` | `GCXtBIZJoFY` |

## remove | `_ZN3WTF5Mutex6unlockEv` | `qr+T1Bp3AqE` |

## remove | `_ZN3WTF5Mutex7tryLockEv` | `fFZmV5BzYC0` |

## remove | `_ZN3WTF5MutexC1Ev` | `zFwxBZREkG8` |

## remove | `_ZN3WTF5MutexC2Ev` | `ut8I892aPW8` |

## remove | `_ZN3WTF5MutexD1Ev` | `SVYZa9hbTh8` |

## remove | `_ZN3WTF5MutexD2Ev` | `WBxQfbet+nY` |
