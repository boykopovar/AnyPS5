# libharfbuzz

## add Functional

- [Layout query guest pointer validation](../../../core/libs/prx/libharfbuzz/Layout.cpp) (libharfbuzz) - `hb_ot_layout_table_get_script_tags`, `hb_ot_layout_script_get_language_tags`, and `hb_ot_layout_language_find_feature` validate and transfer guest `count`, `tags`, and `index` pointers through `GuestCopyIn_nid_no_patch`, `GuestWritable_nid_no_patch`, and `GuestCopyOut_nid_no_patch` in addition to `GuestArena::HostWrite` while preserving `errno`, rejecting invalid or unwritable pointers via `NotImplemented_nid_no_patch`; console behavior on faulted pointers is unverified.
