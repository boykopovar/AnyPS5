# libSceMusicCoreInterface technical debt

### Unknown function info

- [sceMusicCoreIfInitializeInterface, sceMusicCoreIfSetFunctionTable, sceMusicCoreIfMainLoop, sceMusicCoreIfSetInternalFunctionTable, sceMusicCoreIfSetFunctionTableExt2, sceMusicCoreIfSetFunctionTableUnlimited and sceMusicCoreIfInternalInitializeInterface](../../../core/libs/prx/libSceMusicCoreInterface/Export.cpp) - names and NIDs are present in the [PS5 13.60 export inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libscemusiccoreinterface-0263dcc276.jsonl). Parameter lists, return types and behavior are unverified; the parameterless `int` declarations are import-resolution placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. No MusicCore interface behavior is implemented.
