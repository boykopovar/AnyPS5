# libharfbuzz

## add Functional

- [hb_ft_face_create_cached guest pointer guard](../../../core/libs/prx/libharfbuzz/Export.cpp) (libharfbuzz) - Console fault behavior for invalid or read-only `FT_Face` records passed to `hb_ft_face_create_cached` is unmeasured; guest reads now prepare and verify readability (`GuestArenaPrepareHostRead_nid_no_patch` + `GuestReadable_nid_no_patch`) before `GuestCopyIn_nid_no_patch`, and updates to `generic` hold an active `GuestArenaBeginHostWrite_nid_postfix`/`EndHostWrite` guard across `GuestCopyOut_nid_no_patch` while preserving `errno`, calling `NotImplemented_nid_no_patch` on inaccessible or unwritable guest memory. Closes when: console fault behavior for `hb_ft_face_create_cached` is verified on hardware.
