# libSceVdecShevc technical debt

### Unknown function info

- [sceSdecCreateSwHevc, sceSdecDestroySwHevc, sceSdecExecuteSwHevc, sceSdecGetFrameSwHevc, sceSdecQueryMemorySwHevc and sceSdecResetSwHevc](../../../core/libs/prx/libSceVdecShevc/Export.cpp) (libSceVdecShevc) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceVdecShevc.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
- [sceSdecGetVersionSwHevc](../../../core/libs/prx/libSceVdecShevc/Export.cpp) (libSceVdecShevc) - the prototype and behaviour follow firmware 13.42's libSceVdecShevc.sprx, read from disassembly and run under an emulator, not measured on a console: it takes a `uint32_t*`, writes 0x13590001 there unless the pointer is null, and returns nothing; the .native variant writes the same value and is not implemented here

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceVdecShevc/Export.cpp) - 6 entries present in both version-labelled images are import-resolution declarations only. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `79qqGAVnWfA` |
| `Unknown` | `PTO17yTeoSk` |
| `Unknown` | `hZXstu74B4I` |
| `Unknown` | `lLaAHswK1zQ` |
| `Unknown` | `wvq+GPpswaw` |
| `Unknown` | `xbRElcEVmdM` |
