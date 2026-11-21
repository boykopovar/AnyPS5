# libSceNKWebKit

## add Functional

- [bmalloc::IsoTLS::scavenge, bmalloc::Scavenger::schedule and scheduleIfUnderMemoryPressure](../../../core/libs/prx/libSceNKWebKit/src/Bmalloc.cpp) (libSceNKWebKit) - not compared with a console module. They return without doing anything, like `bmalloc::api::scavenge` and `disableScavenger`. In [IsoTLS.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531a/Source/bmalloc/bmalloc/IsoTLS.cpp) `scavenge` only visits the calling thread's `IsoTLS`, which `IsoTLS::ensureEntries` creates and which is still a throwing stub here, so no thread has one. In [Scavenger.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531a/Source/bmalloc/bmalloc/Scavenger.cpp) both `schedule` calls only wake the scavenger thread to return freed bmalloc pages to the system, and libSceNKWebKit has no bmalloc heaps or scavenger thread: its memory comes from the host allocator and goes back to it on free. Closes when: compared with a console module.

## remove `_ZN7bmalloc6IsoTLS8scavengeEv`

## remove `_ZN7bmalloc9Scavenger29scheduleIfUnderMemoryPressureEm`

## remove `_ZN7bmalloc9Scavenger8scheduleEm`
