# libSceNpTrophy2

## remove [sceNpTrophy2GetGameIcon]

## remove [sceNpTrophy2GetRewardIcon]

## change [Trophy queries]

- [Trophy queries](../../../core/libs/prx/libSceNpTrophy2/src/TrophyInfo.cpp) - not confirmed on the console: `sceNpTrophy2GetTrophyInfo` of a trophy id that is not in the title's [trophy data](../../../core/libs/prx/libSceNpTrophy2/src/TrophyPackage.cpp) (`sce_sys/trophy2/trophyNN.ucp`) returns `0x8055390A` (KytyPS5's `NP_TROPHY2_ERROR_INVALID_TROPHY_ID`) and writes nothing; a context not created or destroyed reports placeholders and no unlocks and returns success; hidden trophies report their names and descriptions before the unlock; `progress_percentage` weighs gold 6, silver 2 and bronze 1 and leaves the platinum out, as KytyPS5 does (the ratio of the published PSN trophy points). A group that is not in the data throws. Without the file, or when it is malformed, the queries report the old placeholders and `sceNpTrophy2GetTrophyInfoArray` lists only trophy 0. Unlocks come from the [trophy store](../../../core/libs/prx/libSceNpTrophy2/src/TrophyStore.cpp).

## add Functional

- [Trophy icons](../../../core/libs/prx/libSceNpTrophy2/src/TitleTrophies.cpp): the names come from the `trophy00.ucp` of Unity's PSN sample app ([NPWR18437_00 in PS5TrophyExtract0r](https://github.com/LiEnby/PS5TrophyExtract0r/blob/78a3589ecaeaa35a4c88c02f974f985ef9192cb9/Examples/trophy00.ucp)): `icon0_en-US.png`, `gr0001_en-US.png`, `trop0000.png` to `trop0007.png`, and `reward0007.png` for its one trophy with a reward, so a reward icon is looked up by trophy id. The game and group icons follow the text language order; whether trophy or reward icons can carry one is not known. The signatures and the size query with a null buffer follow PPSA06888's `PSNCore.prx` and SharpProspero. Not confirmed on the console: a missing icon, group -1's included (the console may give the game icon), returns `SCE_NP_TROPHY2_ERROR_ICON_FILE_NOT_FOUND` with the size zeroed, and a locked trophy that is not hidden gives its icon. A locked hidden trophy's icon and a buffer smaller than the icon throw.
