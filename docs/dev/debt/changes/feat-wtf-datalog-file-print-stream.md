# libSceJsc

## add Functional

- [WTF data log sink](../../../core/libs/prx/libSceJsc/src/WtfDataLog.cpp) (libSceJsc) - not compared with a console module. Follows the PlayStation branch of the pinned [DataLog.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/DataLog.cpp#L114-L179) and FilePrintStream.cpp; libSceJsc owns the stream and libSceNKWebKit's `dataFile` and `FilePrintStream` destructors call into it. The 24-byte `FilePrintStream` layout is source-derived. The `FilePrintStream` and `LockedPrintStream` vtables are made here without RTTI, and the locked stream uses a host recursive mutex instead of `RecursiveLockAdapter<WordLock>`. `open` allocates with the guest `operator new` and the deleting destructor frees with `operator delete` where WebKit uses `fastMalloc` and `fastFree`; the console allocator is unmeasured. A failed `setDataFile` `RELEASE_ASSERT` and deleting the static locked stream throw instead of crashing.

## remove | `_ZN3WTF11setDataFileEOSt10unique_ptrINS_11PrintStreamESt14default_deleteIS1_EE` | `G4Jr+rsslPg` |

## remove | `_ZN3WTF15FilePrintStream4openEPKcS2_` | `g0vmm9VmCFY` |

# libSceNKWebKit

## remove | `_ZN3WTF15FilePrintStreamD0Ev` | `9yaO7ovhE7o` |

## remove | `_ZN3WTF15FilePrintStreamD1Ev` | `f4A+i2Tlrmk` |

## remove | `_ZN3WTF15FilePrintStreamD2Ev` | `2o3n0iKkjwY` |

## remove | `_ZN3WTF8dataFileEv` | `iZD+UvpXBfg` |
