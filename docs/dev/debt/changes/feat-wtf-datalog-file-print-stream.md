# libSceJsc

## add Functional

- [WTF data log sink](../../../core/libs/prx/libSceJsc/src/WtfDataLog.cpp) (libSceJsc) - not compared with a console module. Follows the PlayStation branch of the pinned [DataLog.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/DataLog.cpp#L56-L179); libSceJsc owns the stream for libSceNKWebKit. The 24-byte `FilePrintStream` layout is source-derived. The vtables have no RTTI and the lock is a host recursive mutex, not `RecursiveLockAdapter<WordLock>`. `open` and D0 use guest `operator new`/`delete`, not `fastMalloc`/`fastFree`. Throws instead of crashing: a failed `setDataFile` `RELEASE_ASSERT`, `dataFile()` after `setDataFile` installed a null stream, and destroying the static locked stream (D0 or D1), which upstream never does.

## remove | `_ZN3WTF11setDataFileEOSt10unique_ptrINS_11PrintStreamESt14default_deleteIS1_EE` | `G4Jr+rsslPg` |

## remove | `_ZN3WTF15FilePrintStream4openEPKcS2_` | `g0vmm9VmCFY` |

# libSceNKWebKit

## remove | `_ZN3WTF15FilePrintStreamD0Ev` | `9yaO7ovhE7o` |

## remove | `_ZN3WTF15FilePrintStreamD1Ev` | `f4A+i2Tlrmk` |

## remove | `_ZN3WTF15FilePrintStreamD2Ev` | `2o3n0iKkjwY` |

## remove | `_ZN3WTF8dataFileEv` | `iZD+UvpXBfg` |
