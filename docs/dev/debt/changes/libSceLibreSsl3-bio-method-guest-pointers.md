# libSceLibreSsl3

## add Unknown function info

- [BIO_meth_new, BIO_meth_set_write, BIO_meth_set_read, BIO_meth_set_ctrl, BIO_meth_set_create, BIO_meth_set_destroy](../../../core/libs/prx/libSceLibreSsl3/src/BioMethod.cpp) (libSceLibreSsl3) - guest `BIO_METHOD` writes are validated through `GuestArena::HostWrite`, `GuestWritable_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving `errno`, throwing `std::invalid_argument` and freeing a newly allocated record on unwritable memory instead of faulting; console behavior for invalid pointers is unverified.
