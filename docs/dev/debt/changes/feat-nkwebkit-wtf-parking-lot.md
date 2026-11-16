# libSceNKWebKit

## add Functional

- [WTF::ParkingLot, Lock, WordLock, ReadWriteLock and BinarySemaphore](../../../core/libs/prx/libSceNKWebKit/src/WtfParkingLot.cpp) (libSceNKWebKit) - not compared with a console module. Ported from [ParkingLot.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531aeddc11259e8515671cf5188e038d/Source/WTF/wtf/ParkingLot.cpp) and the lock sources of the 2021 revision whose ABI the module's WTF exports match; layouts and the `ParkResult`/`UnparkResult` returns were measured on a Linux build of it. Parked threads wait on host mutexes in a fixed table of 1024 buckets instead of the table that grows with the thread count, so `mayHaveMoreThreads` is set more often when unrelated addresses share a bucket. `forEachImpl` still throws: it passes each parked `WTF::Thread`, which is not implemented. A null object, lambda or time throws.

## remove | `_ZN3WTF10ParkingLot11unparkCountEPKvj` | `o1Tw6r1096E` |

## remove | `_ZN3WTF10ParkingLot13unparkOneImplEPKvRKNS_12ScopedLambdaIFlNS0_12UnparkResultEEEE` | `spOa93PWZk8` |

## remove | `_ZN3WTF10ParkingLot21parkConditionallyImplEPKvRKNS_12ScopedLambdaIFbvEEERKNS3_IFvvEEERKNS_24TimeWithDynamicClockTypeE` | `NRUy9MCPBJI` |

## remove | `_ZN3WTF10ParkingLot9unparkAllEPKv` | `ocD3Wh0yT1E` |

## remove | `_ZN3WTF10ParkingLot9unparkOneEPKv` | `56Wsrmx3txk` |

## remove | `_ZN3WTF13ReadWriteLock10readUnlockEv` | `yPRGiJGWA-4` |

## remove | `_ZN3WTF13ReadWriteLock11writeUnlockEv` | `trYWw7dUt9Y` |

## remove | `_ZN3WTF13ReadWriteLock8readLockEv` | `tbhRqP+K-1c` |

## remove | `_ZN3WTF13ReadWriteLock9writeLockEv` | `cKaXv3V+n+k` |

## remove | `_ZN3WTF15BinarySemaphore9waitUntilERKNS_24TimeWithDynamicClockTypeE` | `JqN5MQOaJVs` |

## remove | `_ZN3WTF4Lock10unlockSlowEv` | `e4I+fGvwugs` |

## remove | `_ZN3WTF4Lock13safepointSlowEv` | `UX5VJ-OZdTg` |

## remove | `_ZN3WTF4Lock8lockSlowEv` | `GciZ5UdsizU` |

## remove | `_ZN3WTF8WordLock10unlockSlowEv` | `MffI7wLS6TM` |

## remove | `_ZN3WTF8WordLock8lockSlowEv` | `Acl7E8U+V44` |
