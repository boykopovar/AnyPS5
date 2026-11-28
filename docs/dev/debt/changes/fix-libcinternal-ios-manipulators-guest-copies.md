# libSceLibcInternal

## add Functional

- [setiosflags, resetiosflags and setbase manipulator functions](../../../core/libs/prx/libSceLibcInternal/IosBase.cpp) (libSceLibcInternal) read the stream's format flags with `GuestCopyIn_nid_no_patch` and write them back with `GuestCopyOut_nid_no_patch`, so a stream that can't be read or written throws `std::invalid_argument`, as a null one does. This is a host-safety fallback; the console's behaviour for an inaccessible stream is not measured.
