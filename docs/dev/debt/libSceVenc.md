# libSceVenc technical debt

### Functional

- [FieldAt, PointerFieldIsNull, CheckRegion, sceVencCreateEncoder and sceVencQueryPreset2](../../../core/libs/prx/libSceVenc/Export.cpp) (libSceVenc) - returning VencError(0x00) for unmapped encoder pointers and VencError(0x01) or VencError(0x04) for unmapped parameter or preset structures via GuestCopyIn/Out_nid_no_patch is a host-safety fallback preserving host errno; console behavior on unmapped memory is unmeasured.
- [Encoder engine](../../../core/libs/prx/libSceVenc/Export.cpp): every export except `39+oWg0ZObY` throws through `NotImplemented_nid_no_patch` once its argument checks pass. The 13.42 module (`libSceVenc.sprx`, sha256 3730676e600d79b5a96891e58f60ab7842e21bbf6bb11d552cda42119b78bfb4) passes the checked arguments to its own encoder engine: parameter and preset conversion, memory sizing, header generation and the encoder object behind the handle. None of it is modelled, so no encoder can be created, and every call on a non-null handle throws after its checks. The checks and their `0x80C700xx` codes were read from disassembly up to the first engine call, not measured on a console.

### Unknown function info

- [Structure layouts](../../../core/libs/prx/libSceVenc/Export.cpp): argument counts come from the registers each 13.42 export reads. Only the fields the checks read are known: pointers at +8, +0x10, +0x18 and +0x28 of the parameters, pointers at +8, +0x10 and +0x18 of the memory description, a 64-bit size at +0 of presets (0x30 to 0x38) and mapped regions (0x20), a pointer at +8 of a decoded frame, and a 32-bit value at +0x20 of the structure at parameters +8 (2 makes +0x28 required). The rest of each layout, and what the `39+oWg0ZObY` types 100 and 200 and its main SoC id threshold 0x840F80 stand for, are unknown.

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceVenc/Export.cpp) - the 13.42 module exports these NIDs beside the named ones. Unidentified names use literal NID aliases. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `sceVencUnmapMemory` | `i5yjeSwjlAY` |
| `Unknown` | `6nGfjRPqCUM` |
| `Unknown` | `B5iyzWMm+DY` |
| `Unknown` | `Vm5uPoERWmg` |
| `Unknown` | `JoBLKJB-Nck` |
| `Unknown` | `766UkIIwtxU` |
| `Unknown` | `2FTI1YJYNBw` |
| `Unknown` | `39+oWg0ZObY` |
