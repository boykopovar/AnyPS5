# libSceNKWebKit

## add Functional

- [WTF::setDataFile(const char*)](../../../core/libs/prx/libSceNKWebKit/src/WtfDataFile.cpp) (libSceNKWebKit) - not compared with a console module. Follows the PlayStation branch of [DataLog.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/DataLog.cpp): the first `%pid` becomes the guest `getpid` within 1024 characters, the file opens with mode `w` through the [Jsc data log](../../../core/libs/prx/libSceJsc/src/WtfDataLog.cpp) and is made unbuffered, and a null path or a failed open falls back to the borrowed guest stdout. As upstream, the replaced stream is flushed but neither closed nor destroyed, and the lock of the shared `LockedPrintStream` is constructed again in place.

## remove `_ZN3WTF11setDataFileEPKc`
