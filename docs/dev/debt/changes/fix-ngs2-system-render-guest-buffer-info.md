# libSceNgs2.native

## add Functional

- [sceNgs2SystemRender guest buffer descriptors](../../../core/libs/prx/libSceNgs2.native/src/System.cpp) (libSceNgs2.native) - the `Ngs2RenderBufferInfo` array is copied with `ReadGuestBytes` before any element is read; an unreadable non-null array throws the same invalid-argument exception as a null array. This is a host-safety fallback; console behavior for an unmapped descriptor array is unmeasured. The nested output-buffer pointers continue through the existing render helpers and are not changed here.
