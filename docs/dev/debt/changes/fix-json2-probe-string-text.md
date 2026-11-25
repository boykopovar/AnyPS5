# libSceJson2

## add Unknown function info

- [String text methods and search](../../../core/libs/prx/libSceJson2/Export.cpp) (libSceJson2) - `String(const char*)`, `String::append`, `String::compare`, `String::operator==`, `String::find`, and `String::rfind` probe guest text with `GuestCopyIn_nid_no_patch` via `GuestTextLength` and `GuestBufferReadable` before accessing memory; unreadable text makes an empty string in the constructor, throws `invalid_argument` in mutating or comparing methods like null text, returns false for equality, and returns `npos` in search helpers, because console behavior on unmapped guest memory is unmeasured.
