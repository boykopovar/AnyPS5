# libharfbuzz

## add Functional

- [hb_ft_face_create_cached](../../../core/libs/prx/libharfbuzz/Export.cpp) copies guest `FT_FaceRec`, `GuestFace`, and `generic` fields through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving `errno`, rejecting unreadable or unwritable guest pointers via `NotImplemented_nid_no_patch(__func__)`.
