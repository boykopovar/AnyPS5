# libSceJson2

## add Unknown function info

- [String text methods and search](../../../core/libs/prx/libSceJson2/Export.cpp) (libSceJson2) - `String(const char*)`, `String::append(const char*)`, `String::append(const char*, size_t)`, `String::operator==(const char*)`, `String::compare(const char*)`, `String::find`, and `String::rfind` probe guest text with `GuestCopyIn_nid_no_patch` into a stack buffer before reading; unreadable text makes an empty string like null text in the constructor, throws `std::invalid_argument` in `append` and `compare`, and returns failure in equality and search, because the console's behaviour on unmapped memory is unmeasured.
