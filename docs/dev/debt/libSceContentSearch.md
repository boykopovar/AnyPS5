# libSceContentSearch technical debt

### Unknown function info

- [sceContentBinderEnd, sceContentBinderGetNumOfDifference, sceContentBinderInitialize, sceContentBinderIsUpdated, sceContentBinderReset, sceContentBinderSearchDifference, sceContentBinderStart, sceContentBinderTerminate, sceContentSearchGetApplicationLastUpdateId, sceContentSearchGetMyApplicationIndex, sceContentSearchGetNumOfApplication and sceContentSearchSearchApplication](../../../core/libs/prx/libSceContentSearch/Export.cpp) (libSceContentSearch) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceContentSearch.c), but no credible prototypes were found in the cited sources. They are declared without parameters solely to resolve known imports and make no ABI claim.

### Functional

- [Content queries](../../../core/libs/prx/libSceContentSearch/Export.cpp) use an empty host library and always report update id 0; the console's initial update id is unverified. No content rows are produced. Every title is assumed to have search permission. Nested filter/sort validation is incomplete: empty nested column sets, null filter values and unknown enum values throw. Guest heap allocation and the platform-dependent minimum heap size are unimplemented. Initialization and outer pointer/count/limit validation follow the supplied module.
- [Metadata access](../../../core/libs/prx/libSceContentSearch/Export.cpp) cannot open metadata by path or content id because missing-content errors remain unresolved. No metadata handles are issued. Queries reject invalid outer pointers but still throw for valid pointers because errors depend on the requested field; closing an unopened handle returns the confirmed invalid-argument error.
