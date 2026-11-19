# libcairo

## change Cairo dereferences these null pointers

- [Surface.cpp](../../../core/libs/prx/libcairo/Surface.cpp) and [Context.cpp](../../../core/libs/prx/libcairo/Context.cpp) - Cairo dereferences these null pointers, and console behaviour is unmeasured. A null surface makes `cairo_surface_status` return `CAIRO_STATUS_NULL_POINTER`, and a null context makes `cairo_path_extents`, `cairo_fill_extents`, `cairo_stroke_extents` and `cairo_clip_extents` store zeros and `cairo_in_stroke` return 0, as Cairo answers for an object in error. A null surface in `cairo_surface_create_similar` throws `std::runtime_error`, and a null surface in `cairo_surface_create_for_rectangle` or null dashes with a positive count in `cairo_set_dash` throw through `NotImplemented_nid_no_patch`, because Cairo has no error object to return for them. Every other null handle, key or callback reaches Cairo unchanged.
