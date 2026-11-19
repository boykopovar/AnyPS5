# libfontconfig

## change [Faulting arguments]

- [Faulting arguments](../../../core/libs/prx/libfontconfig/Objects.cpp) throw `std::runtime_error` for null arguments that Fontconfig 2.15.0 dereferences. A null pattern with an object set in `FcPatternFilter`, which 2.15.0 dereferences only for a nonempty set, throws through `NotImplemented_nid_no_patch`. `FcPatternReference(nullptr)` throws through `NotImplemented_nid_no_patch` and `FcPatternDestroy(nullptr)` does nothing as host-safety choices; the firmware 13.42 disassembly in the review above dereferences both at `0x9f0` and `0x7e0` respectively. Native pointer faults are not reproduced; inaccessible non-null pointers and stale handles are not validated.
