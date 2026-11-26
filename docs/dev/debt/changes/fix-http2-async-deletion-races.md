# libSceHttp2

## add Functional

- [libSceHttp2 asynchronous deletion races](../../../core/libs/prx/libSceHttp2/Export.cpp) - Console scheduling of submission registration against request deletion and of a resolved waiter against the final abort publication is unmeasured. Host submissions reject an already deleted or retired state with the existing invalid-ID result; a resolved waiter does not treat a deletion still publishing its abort as idle. Deterministic CPU tests cover these host interleavings, not console timing or other asynchronous failure paths.
