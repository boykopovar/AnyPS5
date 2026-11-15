# libSceLibcInternal

## add Unknown function info

- [setprogname name](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - the name is copied in with `CopyGuestString_nid_no_patch` (up to 64 KiB) to find its last `/`, and the program name still points into the caller's string; an unreadable name throws the invalid-argument exception a null name already throws and keeps the current name, because the console's behaviour on unmapped memory is unmeasured.
