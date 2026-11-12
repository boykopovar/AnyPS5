# libSceContentSearch technical debt

### Unknown function info

- [libSceContentSearch additional exports](../../../core/libs/prx/libSceContentSearch/Export.cpp) - these name/NID pairs are present in the 13.60 export inventory. Parameter lists, return types and behavior are unverified. Parameterless `int` declarations are import-resolution placeholders, not verified ABI prototypes; every entry throws through `NotImplemented_nid_no_patch`. No runtime functionality is implemented.

| Name | NID |
| --- | --- |
| `sceContentSearchAddAvContentCreatedEventCondition` | `HzLPPnuWTek` |
| `sceContentSearchDisableAvContentCreatedEvent` | `cb8Dl69jk7A` |
| `sceContentSearchEnableAvContentCreatedEvent` | `vlOqhFPOLDs` |
| `sceContentSearchGetContentIdByAvContentCreatedEvent` | `2Dj56xvkS5M` |

- [sceContentBinderEnd, sceContentBinderGetNumOfDifference, sceContentBinderInitialize, sceContentBinderIsUpdated, sceContentBinderReset, sceContentBinderSearchDifference, sceContentBinderStart, sceContentBinderTerminate and sceContentSearchSearchApplication](../../../core/libs/prx/libSceContentSearch/Export.cpp) (libSceContentSearch) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceContentSearch.c), but no credible prototypes were found in the cited sources. They are declared without parameters solely to resolve known imports and make no ABI claim.

### Functional

- [sceContentSearchInit, sceContentSearchGetApplicationLastUpdateId](../../../core/libs/prx/libSceContentSearch/Export.cpp) (libSceContentSearch) - returning ErrorInvalidArgument (0x809D1003) for unmapped parameter or unwritable output pointers is a host-safety fallback; console fault behavior is unmeasured.
- [Content queries](../../../core/libs/prx/libSceContentSearch/Export.cpp) use an empty host library and always report update id 0; the console's initial update id is unverified. No content rows are produced. Every title is assumed to have search permission. Nested filter/sort validation is incomplete: empty nested column sets, null filter values and unknown enum values throw. Guest heap allocation and platform-dependent minimum heap size are unimplemented. Initialization and outer pointer/count/limit validation follow the supplied module. In `sceContentSearchGetTotalContentSize`, `sceContentSearchGetNumOfContent` and `sceContentSearchGetContentLastUpdateId`, returning `ErrorInvalidArgument` for non-null unmapped pointers is an unmeasured host-safety fallback; in `GetTotalContentSize` and `GetNumOfContent`, the first output is stored before the second pointer is copied out, so if only the second pointer is unwritable, the first value is stored when `ErrorInvalidArgument` is returned.
- [sceContentSearchGetNumOfApplication, sceContentSearchGetApplicationLastUpdateId and sceContentSearchGetMyApplicationIndex](../../../core/libs/prx/libSceContentSearch/Export.cpp) (libSceContentSearch) - signatures from the 11.00 module wrappers at `0x18e70`, `0x18f70` and `0x191a0` in `libSceContentSearch.sprx` (SHA-256 `af44252b7b9a0c2d83d01197a4d38119375fb14b82c7e2a822187a49432b42e4`): `GetNumOfApplication` takes the column-set and order-by pairs plus the count and update-id outputs; `GetApplicationLastUpdateId` writes a 64-bit update id. The host reports zero applications and update id 0 because the host library has no application records; the console values are unverified. `GetMyApplicationIndex` writes a 32-bit index, defaults it to 0 before delegating, and returns the delegate's result, which is not established when the title is absent from the library, so the host writes 0 and then throws through `NotImplemented_nid_no_patch`. All three check initialization and null outputs first, and `GetNumOfApplication` validates the column-set and order-by contents as the content queries do. `sceContentSearchSearchApplication` still throws because its eleven-argument contract is not mapped.
- [sceContentSearchSearchContent and sceContentSearchGetNumOfApplication](../../../core/libs/prx/libSceContentSearch/Export.cpp) - console behavior for unwritable non-null count or update-id outputs is unmeasured. AnyPS5 returns `ErrorInvalidArgument` as a host-safety fallback; when only the second output is unwritable, the first output has already been written. `SearchContent` still leaves the `infos` buffer untouched because the host library has no content rows.
- [Metadata access](../../../core/libs/prx/libSceContentSearch/Export.cpp) cannot open metadata by path or content id because missing-content errors remain unresolved: past their argument checks, both opening calls go through `NotImplemented_nid_no_patch`. No metadata handles are issued. The field-info and value queries reject invalid outer pointers, then go through `NotImplemented_nid_no_patch`, as no metadata id is ever opened and the console's error for one depends on the requested field; closing an unopened handle returns the confirmed invalid-argument error.

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceContentSearch/Export.cpp) - 2 entries observed in the 3.20 export inventory and present in the later 13.60-labelled inventory are import-resolution declarations only. The later inventory label does not authenticate a retail firmware version. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `Np8cEpyDFRE` |
| `Unknown` | `X9e+hZfue0U` |
