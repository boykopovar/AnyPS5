# libSceNKWebKit

## add Functional

- [WTF::MetaAllocator and MetaAllocatorHandle](../../../core/libs/prx/libSceNKWebKit/src/WtfMetaAllocator.cpp) (libSceNKWebKit) - not compared with a console module. `dump` still throws: it needs `PrintStream`. A contended allocator lock goes through `Lock::lockSlow`, which is still a stub. `debugFreeSpaceSize` sums the free space like a debug build instead of crashing, and `shrink` throws when asked to grow. A null object throws. Ported from [MetaAllocator.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531aeddc11259e8515671cf5188e038d/Source/WTF/wtf/MetaAllocator.cpp) and RedBlackTree.h of the 2021 revision whose ABI the module's WTF exports match; sizes and vtable order were measured on a Linux build of it, and random allocate/release/shrink sequences give the same addresses, page notifications and statistics as that build. The three hash maps hold host tables in their pointer slots, as no exported or inline code reads them.

## remove | `_ZN3WTF13MetaAllocator17addFreshFreeSpaceEPvm` | `RPLKRR7vgYE` |

## remove | `_ZN3WTF13MetaAllocator17currentStatisticsERKNS_6LockerINS_4LockEEE` | `3SW2Rs+BCYE` |

## remove | `_ZN3WTF13MetaAllocator17freeFreeSpaceNodeEPNS0_13FreeSpaceNodeE` | `sIQ2j-9PjgU` |

## remove | `_ZN3WTF13MetaAllocator18debugFreeSpaceSizeEv` | `HjS-hWzdVyw` |

## remove | `_ZN3WTF13MetaAllocator19isInAllocatedMemoryERKNS_14AbstractLockerEPv` | `HT5RnWbzRwc` |

## remove | `_ZN3WTF13MetaAllocator7releaseERKNS_6LockerINS_4LockEEERNS_19MetaAllocatorHandleE` | `kwKBptlN1OA` |

## remove | `_ZN3WTF13MetaAllocator8allocateERKNS_6LockerINS_4LockEEEm` | `Mb2fweNBShA` |

## remove | `_ZN3WTF13MetaAllocatorC2ERNS_4LockEmm` | `7GF7CfOKTBw` |

## remove | `_ZN3WTF13MetaAllocatorD0Ev` | `bWPSP8A71Rk` |

## remove | `_ZN3WTF13MetaAllocatorD2Ev` | `55-0EcgDmz8` |

## remove | `_ZN3WTF19MetaAllocatorHandle6shrinkEm` | `gsZfTYKMm7o` |

## remove | `_ZN3WTF19MetaAllocatorHandleD1Ev` | `xECsbsDabJE` |

## remove | `_ZN3WTF19MetaAllocatorHandleD2Ev` | `JbEwAVpz8zE` |
