# libSceNpTrophy2

## add Functional

- [Icon size guest output](../../../core/libs/prx/libSceNpTrophy2/src/Context.cpp) (libSceNpTrophy2) - `sceNpTrophy2GetRewardIcon`, `sceNpTrophy2GetGameIcon`, `sceNpTrophy2GetGroupIcon` and `sceNpTrophy2GetTrophyIcon` write `*size` via `GuestCopyOut_nid_no_patch` and throw `std::invalid_argument` with caller `errno` preserved when unwritable; console fault handling is unmeasured.
