# libSceContentSearch technical debt

### Unknown function info

- [libSceContentSearch additional exports](../../../core/libs/prx/libSceContentSearch/Export.cpp) - these name/NID pairs are present in the 13.60 export inventory. Parameter lists, return types and behavior are unverified. Parameterless `int` declarations are import-resolution placeholders, not verified ABI prototypes; every entry throws through `NotImplemented_nid_no_patch`. No runtime functionality is implemented.

| Name | NID |
| --- | --- |
| `sceContentSearchAddAvContentCreatedEventCondition` | `HzLPPnuWTek` |
| `sceContentSearchDisableAvContentCreatedEvent` | `cb8Dl69jk7A` |
| `sceContentSearchEnableAvContentCreatedEvent` | `vlOqhFPOLDs` |
| `sceContentSearchGetContentIdByAvContentCreatedEvent` | `2Dj56xvkS5M` |

- [sceContentBinderEnd, sceContentBinderGetNumOfDifference, sceContentBinderInitialize, sceContentBinderIsUpdated, sceContentBinderReset, sceContentBinderSearchDifference, sceContentBinderStart, sceContentBinderTerminate, sceContentSearchGetApplicationLastUpdateId, sceContentSearchGetMyApplicationIndex, sceContentSearchGetNumOfApplication and sceContentSearchSearchApplication](../../../core/libs/prx/libSceContentSearch/Export.cpp) (libSceContentSearch) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceContentSearch.c), but no credible prototypes were found in the cited sources. They are declared without parameters solely to resolve known imports and make no ABI claim.

### Functional

- [Content queries](../../../core/libs/prx/libSceContentSearch/Export.cpp) use an empty host library and always report update id 0; the console's initial update id is unverified. No content rows are produced. Every title is assumed to have search permission. Nested filter/sort validation is incomplete: empty nested column sets, null filter values and unknown enum values throw. Guest heap allocation and the platform-dependent minimum heap size are unimplemented. Initialization and outer pointer/count/limit validation follow the supplied module.
- [Metadata access](../../../core/libs/prx/libSceContentSearch/Export.cpp) cannot open metadata by path or content id because missing-content errors remain unresolved: past their argument checks, both opening calls go through `NotImplemented_nid_no_patch`. No metadata handles are issued. The field-info and value queries reject invalid outer pointers, then go through `NotImplemented_nid_no_patch`, as no metadata id is ever opened and the console's error for one depends on the requested field; closing an unopened handle returns the confirmed invalid-argument error.
