# libSceLibreSsl

## add Unknown function info

- [UI_get0_user_data, UI_get_string_type, UI_get_input_flags](../../../core/libs/prx/libSceLibreSsl/src/UiString.cpp) (libSceLibreSsl) - compute guest field addresses via byte offsets with address-overflow checks and read them through `GuestArenaPrepareHostRead_nid_no_patch`, `GuestReadable_nid_no_patch`, and `GuestCopyIn_nid_no_patch` while preserving `errno`; console fault behavior for invalid guest pointers is not measured.
