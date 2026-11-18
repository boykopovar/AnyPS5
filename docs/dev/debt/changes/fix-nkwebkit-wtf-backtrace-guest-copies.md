# libSceNKWebKit

## add Functional

- [WTFGetBacktrace](../../../core/libs/prx/libSceNKWebKit/src/WtfAssert.cpp) (libSceNKWebKit) reads the requested frame count with `GuestCopyIn_nid_no_patch` and writes the captured count back with `GuestCopyOut_nid_no_patch`. A count that can't be read returns at once, as a null one does. On Windows, a frame buffer that isn't writable for the requested count captures no frames and reports 0, the count every other host reports. This is a host-safety fallback; the console's behaviour for inaccessible pointers is not measured.
