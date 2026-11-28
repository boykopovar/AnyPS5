# libSceEsvmPlayReady technical debt

### Additional catalog exports (PS5 13.60 stub-library catalog)

- [libSceEsvmPlayReady catalog exports](../../../core/libs/prx/libSceEsvmPlayReady/Export.cpp) - `sceEsvmProcessAckResponse`, the export the [PS5 13.60 function stub library catalog](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libsceesvmplayready-5091672932.jsonl) gives as `defined_function_export` with `source_evidence: [firmware_export_table, verified_nid_name_map]`, in a module the repository does not carry. The record says `signature_status: unknown`, so the catalog publishes no prototype and the declaration keeps the no-parameter convention with the body throwing through `NotImplemented_nid_no_patch`. The catalog lists three records for this module; `sceEsvmInitializePlayReady` and `sceEsvmRequestMediaKeySystemAccessPlayReady` are already declared as unbound throw-stubs in libSceEsvm's inventory, with no `APS5_EXPORT` binding, and are not re-declared here. Console behaviour is not measured.

| Name | NID |
| --- | --- |
| `sceEsvmProcessAckResponse` | `cvT7Fv3hQWE` |
