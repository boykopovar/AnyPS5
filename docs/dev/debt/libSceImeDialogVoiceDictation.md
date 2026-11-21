# libSceImeDialogVoiceDictation technical debt

### Additional catalog exports (PS5 13.60 stub-library catalog)

- [libSceImeDialogVoiceDictation catalog exports](../../../core/libs/prx/libSceImeDialogVoiceDictation/Export.cpp) - the single export the [PS5 13.60 function stub library catalog](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libsceimedialogvoicedictation-250c3b1b80.jsonl) gives for this module, which lists exactly one record, as `defined_function_export` with `source_evidence: [firmware_export_table, verified_nid_name_map]`, in a library the repository does not carry. Every catalog NID equals the value the repository's own NID algorithm derives from the name. Every record says `signature_status: unknown`, so the catalog publishes no prototype and the declaration keeps the no-parameter convention and the body throws through `NotImplemented_nid_no_patch`. Console behaviour is not measured.

| Name | NID |
| --- | --- |
| `sceImeDialogSetVoiceDictationEnabled` | `XsiH5BYQpUo` |
