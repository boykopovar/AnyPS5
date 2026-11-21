# libScePngEnc technical debt

### Functional

- [libScePngEnc](../../../core/libs/prx/libScePngEnc/Export.cpp) returns SCE_PNG_ENC_ERROR_INVALID_ADDR for unmapped or unwritable pointer arguments and buffers and SCE_PNG_ENC_ERROR_INVALID_HANDLE for an unreadable handle. This is a defensive host-protection fallback; console behaviour for unmapped or read-only memory is unmeasured. Encoding publishes success metadata only after the PNG copy succeeds; a later metadata-copy failure can leave PNG bytes written. These are host fallback semantics, not measured console partial-output behavior.
- [scePngEncEncode](../../../core/libs/prx/libScePngEnc/Export.cpp): native filter setup always includes the unfiltered candidate, including for partial masks. The host now permits it too; exact filter-selection ties, compressed bytes and partial-output behavior are not matched to the native encoder. Creation validation, work-memory size, 16-byte handle alignment and the context prefix now match the supplied module. Input-buffer alignment, pitch, dimensions, buffer capacity and selected-filter count now follow the native checks. Host scratch storage remains separately allocated.
