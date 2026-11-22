# libSceIcu

## add Unknown function info

- [u_strstr and u_strrstr guest strings](../../../core/libs/prx/libSceIcu/Search.cpp) (libSceIcu) - the substring, then the string, are copied in up to their terminator with `GuestCopyIn_nid_no_patch` in page-bounded chunks, and the result is the guest string plus the match offset; an unreadable string throws `std::invalid_argument` with `errno` preserved, because the console's behaviour on unmapped memory is unmeasured. A null or empty substring returns the string without reading it.
