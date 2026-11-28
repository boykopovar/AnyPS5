# libSceJson2

## add Unknown function info

- [sce::Json::Array and Array::iterator guest-pointer checks](../../../core/libs/prx/libSceJson2/Export.cpp) (libSceJson2) - the 22 `Array` and `Array::iterator` exports read and write their guest `Array`, `ArrayIterator`, `ArrayIteratorState` and `Value` arguments through `GuestCopyIn_nid_no_patch`/`GuestCopyOut_nid_no_patch` and throw `NotImplemented` on inaccessible, page-straddling or read-only guest targets instead of faulting; what the console does on invalid guest pointers is unmeasured. Closes when: console behaviour on invalid `Array` and `Array::iterator` pointers is measured.
