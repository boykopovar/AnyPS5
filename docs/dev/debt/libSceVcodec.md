# libSceVcodec technical debt

### Unknown function info

- [sceVcodecCloseHwBudget, sceVcodecOpenHwBudgetByAppid, sceVcodecSetHwBudgetSize, sceVcodecOpenHwBudget and sceVcodecSetHwBudgetType](../../../core/libs/prx/libSceVcodec/Export.cpp) - names and NIDs are present in the [PS5 13.60 export inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libscevcodec-942ad05a91.jsonl). Parameter lists, return types and behavior are unverified; the parameterless `int` declarations are import-resolution placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Three additional inventory NIDs remain unresolved and are not declared. No hardware-budget behavior is implemented.
