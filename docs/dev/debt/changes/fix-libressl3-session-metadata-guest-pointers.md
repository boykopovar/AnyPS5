# libSceLibreSsl3

## add Functional

- [SSL_SESSION_get_timeout, SSL_SESSION_set_time, SSL_SESSION_get_protocol_version, and SSL_SESSION_get_master_key guest pointer checks](../../../core/libs/prx/libSceLibreSsl3/src/SessionMetadata.cpp) (libSceLibreSsl3) - `SSL_SESSION` field and master-key buffer accesses go through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving `errno`, rejecting faulted guest pointers via `NotImplemented_nid_no_patch(__func__)`; console behavior on faulted pointers is unverified.
