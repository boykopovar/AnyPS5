# libSceFreeTypeOt

## add Functional

- [ft_service_list_lookup guest pointers](../../../core/libs/prx/libSceFreeTypeOt/Export.cpp) (libSceFreeTypeOt) - service descriptor entries and service ID strings are read through `GuestCopyIn_nid_no_patch` while preserving `errno`, rejecting an inaccessible descriptor entry or an unreadable or unterminated ID via `std::invalid_argument` as a host-safety fallback with console behavior unmeasured.
