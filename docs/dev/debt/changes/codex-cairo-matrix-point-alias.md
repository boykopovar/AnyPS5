# libcairo

## change Matrix constructors, transforms and multiplication check guest pointer reads and writes

- [Matrix.cpp](../../../core/libs/prx/libcairo/Matrix.cpp) - A point output that aliases a translation field of its input matrix can differ from pinned Cairo 1.18.0 because the wrapper copies the matrix before transforming the point. Matrix constructors, transforms and multiplication check guest pointer reads and writes; an unreadable or unwritable matrix or coordinate pointer calls `GuestFault_nid_no_patch`, because Cairo 1.18.0 [cairo-matrix.c](https://gitlab.freedesktop.org/cairo/cairo/-/blob/1.18.0/src/cairo-matrix.c) dereferences them without a check. The console's error response to these pointers is unmeasured. Coordinate outputs are checked before any is written, but a concurrent page protection change can still invalidate a checked address.
