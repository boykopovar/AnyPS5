# libSceAgcDriver

## add Functional

- [Retire registry snapshots](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp) (libSceAgcDriver) - the pin waiter copies the registry for a retire pass only when an import it retires has a live unit shadow unit (`RetireShadowReadsRegistered`). Such a pass still copies every registered range, although the coverage test reads only the ranges under that import, and the pass made without the device lock walks the import table up to three times (the overlap scan, the pass, and the pass again once the snapshot is taken). The `[address-sync]` profile line counts both (`retire registry snapshots: N taken, N skipped`); the share of taken ones is not measured on a title. Closes when: the coverage test is answered from the import's own ranges, or the taken snapshots are measured and cost nothing that shows.
