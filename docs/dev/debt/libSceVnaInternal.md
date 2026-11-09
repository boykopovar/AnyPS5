# libSceVnaInternal technical debt

### Unknown function info

- [VnaInternal inventory exports](../../../core/libs/prx/libSceVnaInternal/Export.cpp) - 84 names and NIDs are present in the [PS5 13.60 export inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libscevnainternal-b1ef6786a5.jsonl). Parameter lists, return types and behavior are unverified; the parameterless `int` declarations are import-resolution placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Four additional inventory NIDs remain unresolved and are not declared. No voice-network-agent behavior is implemented. Closed PR #10672 used a PS4-only source for 27 VnaInternal declarations; none of that source is evidence for this entry, and the 21 repeated identities here are independently present in the pinned PS5 13.60 records.
