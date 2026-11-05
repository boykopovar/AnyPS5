# libScePsmlBcSisr technical debt

### Additional catalog exports (PS5 13.60 stub-library catalog)

- [libScePsmlBcSisr catalog exports](../../../core/libs/prx/libScePsmlBcSisr/Export.cpp) - the 3 named callable exports of this library that the [PS5 13.60 function stub library catalog](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libscepsmlbcsisr-88d7de59b5.jsonl) gives as `defined_function_export` with `source_evidence: [firmware_export_table, verified_nid_name_map]`, in a library the repository does not carry. Every catalog NID equals the value the repository's own NID algorithm derives from the name. The catalog publishes no prototypes, so each declaration keeps the no-parameter convention and each body throws through `NotImplemented_nid_no_patch`. Console behaviour is not measured. The catalog lists 6 records for this module; the other 3 carry no name (`A0kDyeWNSvs`, `SfklkVp-lhc`, `ZyqURPTSHl0`), so this library declares no export for them.

| Name | NID |
| --- | --- |
| `scePsmlBcSisrGetInternalBufferRequirement` | `Opts9LtjSfo` |
| `scePsmlBcSisrGetPacketSizeInDwords` | `sIdqI1s+z50` |
| `scePsmlBcSisrInit` | `O4-EydkeC3M` |
