# libScePngEnc technical debt

### Functional

- [scePngEncEncode](../../../core/libs/prx/libScePngEnc/Export.cpp): native filter setup always includes the unfiltered candidate, including for partial masks. The host now permits it too; exact filter-selection ties, compressed bytes and partial-output behavior are not matched to the native encoder. Creation validation, work-memory size, 16-byte handle alignment and the context prefix now match the supplied module. Input-buffer alignment, pitch, dimensions, buffer capacity and selected-filter count now follow the native checks. Host scratch storage remains separately allocated.
