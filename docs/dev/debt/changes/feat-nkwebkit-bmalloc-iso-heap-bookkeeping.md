# libSceNKWebKit

## add Functional

- [bmalloc iso heap bookkeeping](../../../core/libs/prx/libSceNKWebKit/src/BmallocIsoHeap.cpp) (libSceNKWebKit) - not compared with a console module. The `IsoHeapImplBase` constructor, deleting destructor, footprint and freeable counters and `addToAllIsoHeaps`, `AllIsoHeaps::add` and `head`, the `IsoTLSEntry` constructor, deleting destructor and `extent`, and `IsoTLSLayout::add` follow the pinned [bmalloc sources](https://github.com/neonmodder123/PS5-WebKit-11.x/tree/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/bmalloc/bmalloc) built without libpas, on 112- and 24-byte layouts measured from them. `addToAllIsoHeaps` adds to a host `AllIsoHeaps`, because the module's per-process storage is not exported, and the locks are host mutexes. A failed `RELEASE_BASSERT` or a null argument throws. `scavengeNow` and `finishScavenging` still throw: they decommit guest pages. Closes when: compared with a console module.

## remove | `_ZN7bmalloc11AllIsoHeaps3addEPNS_15IsoHeapImplBaseE` | `LEKGLwltPt8` |

## remove | `_ZN7bmalloc11AllIsoHeaps4headEv` | `Da0OiEp6X68` |

## remove | `_ZN7bmalloc11IsoTLSEntryC2Em` | `6-GmQWtFE6Y` |

## remove | `_ZN7bmalloc11IsoTLSEntryD0Ev` | `psjVBg5y6OM` |

## remove | `_ZN7bmalloc12IsoTLSLayout3addEPNS_11IsoTLSEntryE` | `tMWlx2AgEiY` |

## remove | `_ZN7bmalloc15IsoHeapImplBase11didDecommitEPvm` | `skIfr3+tx9E` |

## remove | `_ZN7bmalloc15IsoHeapImplBase13isNowFreeableEPvm` | `QdYNXcesqSo` |

## remove | `_ZN7bmalloc15IsoHeapImplBase16addToAllIsoHeapsEv` | `ezAh2UH+pyE` |

## remove | `_ZN7bmalloc15IsoHeapImplBase18isNoLongerFreeableEPvm` | `LF1S4j5d23g` |

## remove | `_ZN7bmalloc15IsoHeapImplBase9didCommitEPvm` | `Zkl+J182WlE` |

## remove | `_ZN7bmalloc15IsoHeapImplBaseC2ERNS_5MutexE` | `PyFQ+tpKjuc` |

## remove | `_ZN7bmalloc15IsoHeapImplBaseD0Ev` | `mLfgWSYY3pY` |

## remove | `_ZNK7bmalloc11IsoTLSEntry6extentEv` | `1UchwEvKOCY` |
