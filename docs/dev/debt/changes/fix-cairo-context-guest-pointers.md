# libcairo

## add Functional

- [Context guest pointer checks](../../../core/libs/prx/libcairo/Context.cpp) (libcairo) - `cairo_transform`, `cairo_set_matrix`, `cairo_get_matrix`, `cairo_user_to_device`, `cairo_user_to_device_distance`, `cairo_device_to_user`, `cairo_device_to_user_distance`, `cairo_set_dash`, `cairo_get_dash`, `cairo_get_current_point`, `cairo_path_extents`, `cairo_fill_extents`, `cairo_stroke_extents`, and `cairo_clip_extents` validate and transfer non-null guest matrices, dash arrays, coordinates, and extent outputs through `GuestCopyIn_nid_no_patch`, `GuestWritable_nid_no_patch`, and `GuestCopyOut_nid_no_patch` while preserving `errno`, rejecting invalid or unwritable pointers via `NotImplemented_nid_no_patch(__func__)` before writing partial outputs; console behavior on faulted pointers is unverified.
