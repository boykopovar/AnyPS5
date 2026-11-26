# libSceNpFriendListDialog

## change [libSceNpFriendListDialog](../../../core/libs/prx/libSceNpFriendListDialog/Export.cpp)

- [libSceNpFriendListDialog](../../../core/libs/prx/libSceNpFriendListDialog/Export.cpp) - `sceNpFriendListDialogOpenA` and `sceNpFriendListDialogGetResultA` run the common-dialog state machine, show nothing and report user cancellation (no PSN friend list). Result layout beyond the leading `int32_t` code is unknown, so nothing else is written; returning `0x80B8000D` for an unwritable result pointer is an unmeasured host-safety fallback. Signatures follow [Orbital](https://github.com/AlexAltea/orbital/blob/b321fcdbd700cc02da1cb1ad25412781d5ec73c6/tools/ida/db_types.json). Null-pointer check (`COMMON_DIALOG_ERROR_ARG_NULL`) precedes `COMMON_DIALOG_ERROR_NOT_FINISHED`, matching PS5 13.42 `libSceNpFriendListDialog.sprx` (`0xa10`).

# libSceNKWebKitRequirements

## change [Additional callable exports]

- [Additional callable exports](../../../core/libs/prx/libSceNKWebKitRequirements/Export.cpp) - 32 version-labelled entries are throwing import-resolution placeholders (`NotImplemented_nid_no_patch`), absent from the 3.20 catalogue; parameter lists, return types, runtime behavior, retail provenance and data-export layouts/ownership are unverified. Eleven entries resolved via the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) and [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv) use their symbol names; the rest keep literal NID aliases.

# libSceHttp

## change [sceHttpCookieExport and sceHttpCookieImport]

- [sceHttpCookieExport and sceHttpCookieImport](../../../core/libs/prx/libSceHttp/src/Cookies.cpp) (libSceHttp) - console format, replacement-or-merge behaviour, error codes, size bound and import limit checks are unmeasured; the host blob (`APS5CK1`, little-endian length-prefixed fields, creation order, optional expiry) only round-trips here, and a console blob is rejected with `ERROR_INVALID_VALUE`. Export writes the blob (null buffer queries size; short buffer returns `ERROR_OUT_OF_SIZE`). Import replaces the jar; malformed, truncated, oversized or unreadable input returns `ERROR_INVALID_VALUE` or `ERROR_OUT_OF_SIZE` leaving it unchanged. Exceeding cookie-count, per-cookie, per-domain or total-size limits returns `ERROR_OUT_OF_SIZE`; expired cookies are dropped and out-of-range expiries are clamped.

# libSceLibreSsl

## change [BN_new, BN_free, BN_CTX_new

- [BN_new, BN_free, BN_CTX_new, BN_CTX_free, BN_CTX_start, BN_CTX_end, BN_bin2bn, BN_bn2bin, BN_hex2bn, BN_num_bits, BN_set_word, BN_add_word, BN_sub_word, BN_mul and BN_mod_inverse](../../../core/libs/prx/libSceLibreSsl/src/Bignum.cpp) (libSceLibreSsl) - console behaviour is unverified; the 24-byte `BIGNUM` layout (`top`, `dmax`, `neg`, `flags`, 64-bit limbs) and semantics follow LibreSSL 2.x. `BN_CTX` is opaque and only counts `BN_CTX_start`/`BN_CTX_end` (`BN_CTX_get` is unimplemented; `BN_mul` and `BN_mod_inverse` ignore it). `BN_mod_inverse` uses the modulus magnitude and returns 0 for modulus 1 (uncompared with LibreSSL). `BN_hex2bn` preserves negative zero on non-empty digitless hex strings per LibreSSL 2.5.4. Null `BIGNUM` pointers throw (unchecked in LibreSSL). `BN_print` writes one hex digit per `BIO_write`, as `bn_print.c` does.

# libSceAgc

## change [sceAgcDcbSetWorkloadStreamInactive]

- [sceAgcDcbSetWorkloadStreamInactive](../../../core/libs/prx/libSceAgc/DcbState/src/Workload.cpp) (libSceAgc) - writes a placeholder 9-dword `IT_NOP` packet that `Pm4::Execute` skips unread instead of the console's SET_UCONFIG_REG/WRITE_DATA/ATOMIC_MEM sequence below; its size matches firmware 4.03 `libSceAgcDriver.sprx`. Returns null without writing for a null command buffer or unregistered stream. KytyPS5 has no body for it and its active/complete `IT_NOP` layouts differ from OpenAGC's sequence, so neither is taken. The `(buf, streamId)` signature follows OpenAGC's DCB call (OpenAGC says ACB calls share the driver builders). `sceAgcDcbSetWorkloadsActive` checks the stream before reading workload IDs, matching OpenAGC and the console (returned null for stream 1 with IDs at address 2 without faulting).
