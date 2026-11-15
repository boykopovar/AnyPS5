# libSceXml

## add Unknown function info

- [String text constructors](../../../core/libs/prx/libSceXml/Export.cpp) (libSceXml) - `String(const char*)` measures the text with `GuestCopyIn_nid_no_patch` one page at a time into a stack buffer instead of `strlen`, and still keeps the caller's pointer as the view; unreadable text gives a view of size 0, because the console's behaviour on unmapped memory is unmeasured.
