# libSceJsc

## add Unknown function info

- [JSString handle probing](../../../core/libs/prx/libSceJsc/Export.cpp) (libSceJsc) - `JSStringRetain_nid_postfix`, `JSStringGetLength_nid_postfix`, and `JSStringGetCharactersPtr_nid_postfix` probe the `GuestJSString` handle with `GuestCopyIn_nid_no_patch` while preserving `errno`; an unreadable handle throws `std::invalid_argument` in `JSStringRetain_nid_postfix` and returns `0` or `nullptr` in the length and character-pointer queries, because console behavior on unmapped handles is unmeasured.
