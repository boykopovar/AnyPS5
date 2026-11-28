# libcairo

## add Functional

- [Region.cpp](../../../core/libs/prx/libcairo/Region.cpp) - `cairo_region_union_rectangle` and `cairo_region_get_rectangle` copy non-null guest `CairoRectangleInt` pointers through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving `errno`, calling `NotImplemented_nid_no_patch` on unreadable or unwritable pointers instead of crashing the host; console behavior on faulted pointers is unmeasured.
