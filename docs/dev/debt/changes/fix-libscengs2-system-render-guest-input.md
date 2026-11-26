# libSceNgs2.native

## add Functional

- [sceNgs2SystemRender guest input](../../../core/libs/prx/libSceNgs2.native/src/System.cpp) (libSceNgs2.native) - the render buffer list is copied through the guest read helpers before its waveform types and buffer pointers are checked; an unreadable list throws the invalid-argument exception, which is a host-safety fallback, and whether the console faults or returns an argument error on unmapped memory is unmeasured. The channel check, the refusal and silence paths and the render itself use the copied records, so no later step reads the caller's list.
