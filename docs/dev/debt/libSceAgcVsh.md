# libSceAgcVsh technical debt

### Unknown function info

- The 150 declarations in [Export.cpp](../../../core/libs/prx/libSceAgcVsh/Export.cpp) are present in the PS5 3.20 [export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAgcVsh.c) and the local import-stub table. Their signatures and behavior are unverified; the parameterless declarations are for import resolution only and throw through NotImplemented_nid_no_patch.
