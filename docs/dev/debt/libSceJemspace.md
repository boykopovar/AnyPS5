# libSceJemspace technical debt

### Unknown function info

- The thirteen exports in [Export.cpp](../../../core/libs/prx/libSceJemspace/Export.cpp) resolve imports only and throw through `NotImplemented_nid_no_patch`. Each declared name hashes to a defined function NID in the `libSceJemspace` ELF export table. The module firmware version is unverified. The no-parameter declarations do not establish argument layouts or return types. No allocator behavior, memory ownership, allocation failure handling, statistics layout, or console compatibility is implemented or verified.
