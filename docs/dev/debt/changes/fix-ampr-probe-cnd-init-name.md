# libSceAmpr

## add Unknown function info

- [_Cnd_init_with_name name](../../../core/libs/prx/libSceAmpr/Export.cpp) (libSceAmpr) - the name is copied with `CopyGuestString_nid_no_patch` capped at 32 bytes, which keeps the `thrd_error` / `errno` 63 result for 32 or more characters; an unreadable name returns `thrd_error` with `errno` 14 without creating the condition, because the console's behaviour on unmapped memory is unmeasured.
