# libSceDseehx technical debt

### Unknown function info

- [sceDseehxConvert, sceDseehxExtInit, sceDseehxFlush and sceDseehxInit](../../../core/libs/prx/libSceDseehx/Export.cpp) (libSceDseehx) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceDseehx.c), but no credible prototypes were found. Declarations have no parameters solely to resolve known imports; callers must not call them until signatures are verified.

### Functional

- [libSceDseehx](../../../core/libs/prx/libSceDseehx/Export.cpp) tracks lifecycle initialization via atomic state. Uninitialized `sceDseehxFlush` and `sceDseehxConvert` throw `std::logic_error`. `sceDseehxInit` and `sceDseehxExtInit` transition state to initialized (throwing on duplicate init), `sceDseehxFlush` returns 0 when initialized, and initialized `sceDseehxConvert` calls `NotImplemented_nid_no_patch`.
