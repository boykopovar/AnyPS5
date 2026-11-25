# libSceNKWebKit

## change [WTF PlayStation platform defaults]

- [WTF PlayStation platform defaults](../../../core/libs/prx/libSceNKWebKit/src/WtfPlatformDefaults.cpp) - not compared with a console module. `currentSearchLocaleID` and `currentTextBreakLocaleID` query the guest libc `setlocale` with the FreeBSD `LC_MESSAGES` value 6; that libc always reports `C`, so the `""` and `en_us` fallbacks of [TextBreakIteratorInternalICUUnix.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/text/unix/TextBreakIteratorInternalICUUnix.cpp) are untested. The other seven follow the PlayStation branches of the pinned OSAllocatorPlayStation.cpp, FileSystem.cpp, UUID.cpp and Threading.cpp. `OSAllocator::commit`, `decommit` and `releaseDecommitted` replace the `memory_extra::vss` calls of the PS5 11.x OSAllocatorPlayStation.cpp with guest `mprotect` and `munmap`, call `WTFCrash` on failure, ignore `executable`, and keep page contents on `decommit`.

## remove `_ZN3WTF11OSAllocator18releaseDecommittedEPvm`

## remove `_ZN3WTF11OSAllocator6commitEPvmbb`

## remove `_ZN3WTF11OSAllocator8decommitEPvm`
