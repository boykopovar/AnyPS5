# libSceSharePlay

## change The other 20 exports

- [The other 20 exports](../../../core/libs/prx/libSceSharePlay/Export.cpp) (libSceSharePlay) - with no Share Play service the connection state stays below 2, so these exports return `0x810E0004` before and after `sceSharePlayInitialize` and do not read arguments. `6egMR0eB8RU` returns 1, the byte the 13.42 wrapper keeps until the service connects. `sceSharePlaySetProhibition`, the connection-info calls and `sceSharePlayProhibitController` stay as their own entries. Read from the module's disassembly, not measured on a console.

## change sceSharePlaySetShareScreenStatus

- [sceSharePlaySetShareScreenStatus](../../../core/libs/prx/libSceSharePlay/Export.cpp) has an unknown signature. The parameterless `int` declaration is an import-resolution placeholder, not a confirmed ABI. It returns `0x810E0004` before and after initialization, the 13.42 answer while the connection state is not 2. Only its name and NID `zOdmo1vWHIc` are established by the [13.60 export inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libsceshareplay-b821361f17.jsonl); not measured on a console.

## change Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceSharePlay/Export.cpp) - 4 entries observed in the 3.20 export inventory and present in the later 13.60-labelled inventory are import-resolution declarations only. The later inventory label does not authenticate a retail firmware version. Parameter lists and behavior past the disconnected answer are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. They return `0x810E0004`, or 1 for `6egMR0eB8RU`, because the service never connects. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).
