# libSceLibreSsl3

## add Functional

- [tls_config_parse_protocols](../../../core/libs/prx/libSceLibreSsl3/src/TlsProtocols.cpp) copies non-null guest `text` strings via `CopyGuestString_nid_no_patch` (up to 1 MiB) and writes `protocols` via `GuestCopyOut_nid_no_patch` while preserving `errno`, freeing any temporary duplicate buffer and calling `NotImplemented_nid_no_patch` on unreadable, oversized/unterminated or unwritable guest pointers rather than dereferencing them on the host; console behavior on faulted pointers is unverified.
