# libSceJsc

## add Unknown function info

- [JSString creation and comparison](../../../core/libs/prx/libSceJsc/Export.cpp) (libSceJsc) - `JSStringCreateWithUTF8CString_nid_postfix`, `JSStringCreateWithCharacters_nid_postfix`, and `JSStringIsEqualToUTF8CString` probe guest text with `GuestCopyIn_nid_no_patch` before decoding or comparing; unreadable text yields a null string in constructors and returns false in string equality comparison, because console behavior on unmapped memory is unmeasured.
