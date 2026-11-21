# libSceJson2

## add Unknown function info

- [Value text constructor and set](../../../core/libs/prx/libSceJson2/Export.cpp) (libSceJson2) - `Value(const char*)` and `Value::set(const char*)` measure the text with `GuestCopyIn_nid_no_patch` one page at a time into a stack buffer (no host heap, so recovery values keep using only the guest allocator) before building the string; unreadable text makes an empty string like null text in the constructor and throws the null-text exception in `set`, because the console's behaviour on unmapped memory is unmeasured.
