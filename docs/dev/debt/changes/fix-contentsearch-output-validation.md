# libSceContentSearch

## change [Content queries]

- [Content queries](../../../core/libs/prx/libSceContentSearch/Export.cpp) use an empty host library and always report update id 0; the console's initial update id is unverified. No content rows are produced. Every title is assumed to have search permission. Nested filter/sort validation is incomplete: empty nested column sets, null filter values and unknown enum values throw. Guest heap allocation and platform-dependent minimum heap size are unimplemented. Initialization and outer pointer/count/limit validation follow the supplied module. Returning `ErrorInvalidArgument` for an unmapped non-null filter or sort array is an unmeasured host-safety fallback. In `sceContentSearchGetTotalContentSize`, `sceContentSearchGetNumOfContent` and `sceContentSearchGetContentLastUpdateId`, returning `ErrorInvalidArgument` for non-null unmapped pointers is also an unmeasured host-safety fallback.

## change [sceContentSearchSearchContent and sceContentSearchGetNumOfApplication]

- [sceContentSearchSearchContent and sceContentSearchGetNumOfApplication](../../../core/libs/prx/libSceContentSearch/Export.cpp) - console behavior for unwritable non-null count or update-id outputs is unmeasured. AnyPS5 returns `ErrorInvalidArgument` as a host-safety fallback; output destination buffers are validated upfront before writing. `SearchContent` still leaves the `infos` buffer untouched because the host library has no content rows.
