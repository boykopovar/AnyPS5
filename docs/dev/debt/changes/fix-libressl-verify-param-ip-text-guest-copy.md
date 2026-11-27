# libSceLibreSsl

## add Functional

- [X509_VERIFY_PARAM_set1_ip_asc](../../../core/libs/prx/libSceLibreSsl/src/X509VerifyParam.cpp) (libSceLibreSsl) copies its address text in with `CopyGuestString_nid_no_patch` (64 KiB limit) and returns 0, keeping the previous address, for an unreadable or unterminated text, as it does for text that is not an address. This is a host-safety fallback; the console's behaviour for an inaccessible text is not measured. Closes when: compared with the console.
