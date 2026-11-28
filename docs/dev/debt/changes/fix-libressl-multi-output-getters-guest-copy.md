# libSceLibreSsl

## add Functional

- [OCSP response and ALPN getter guest pointers](../../../core/libs/prx/libSceLibreSsl/src/OcspResponse.cpp) - `OCSP_response_status`, `OCSP_single_get0_status` and `SSL_select_next_proto` in [ProtocolSelection.cpp](../../../core/libs/prx/libSceLibreSsl/src/ProtocolSelection.cpp) copy guest inputs and outputs through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` with host `errno` preserved, restoring any already-written outputs before throwing through `NotImplemented_nid_no_patch` when a non-null guest pointer is unreadable or unwritable; console fault behavior is unmeasured.

# libSceLibreSsl3

## add Functional

- [Multi-output key, signature OID and OCSP getter guest pointers](../../../core/libs/prx/libSceLibreSsl3/src/KeyComponents.cpp) - `RSA_get0_key`, `DH_get0_pqg`, `DH_get0_key`, `DSA_get0_pqg`, `DSA_get0_key`, `RSA_get0_factors` and `RSA_get0_crt_params` in [Key.cpp](../../../core/libs/prx/libSceLibreSsl3/src/Key.cpp), `OBJ_find_sigid_algs` in [SignatureAlgorithms.cpp](../../../core/libs/prx/libSceLibreSsl3/src/SignatureAlgorithms.cpp) and `OCSP_resp_find_status` in [OcspFindStatus.cpp](../../../core/libs/prx/libSceLibreSsl3/src/OcspFindStatus.cpp) copy guest inputs and outputs with `errno` preserved and roll back prior outputs before calling `NotImplemented_nid_no_patch` on unmapped or read-only guest memory; console fault behavior is unmeasured.
