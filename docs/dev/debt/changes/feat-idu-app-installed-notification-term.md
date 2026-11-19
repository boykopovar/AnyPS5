# libSceIdu

## remove [sceIduIsAppInstalled and sceIduTerm]

## remove [sceIduGetPkgInstallStatus](../../../core/libs/prx/libSceIdu/Export.cpp)

## change [The early answers]

- [The early answers](../../../core/libs/prx/libSceIdu/Export.cpp) (libSceIdu) - 20 exports return only their argument errors and the uninitialized error: `sceIduInit` throws once its checks pass, so the library is never initialized here. All of the additional exports below are among them. The declarations give only the widths the module checks; what the arguments mean is unknown. In firmware 13.42's `libSceIdu.sprx` (SHA-256 `c86f259763876c82225553653856bb176b332e1ce3c18f473ac2f8bd9fad70f1`), read from disassembly and not run on a console, each returns `0xFFFFFF99` for an argument it rejects and then `0xFFFFFF9A` while the pointer at `0xc030` is null, before any call; the exports of the next entry were read in firmware 9.00 instead. That pointer lies past its segment's file data, so it starts null, and only `sceIduInit` stores a non-null value in it. A field the module reads that cannot be read here throws. The errors' names are unverified.

## add Unknown function info

- [sceIduIsAppInstalled, sceIduNotificationShow, sceIduTerm and the length check of sceIduGetPkgInstallStatus](../../../core/libs/prx/libSceIdu/Export.cpp) (libSceIdu) - their prototypes and checks follow a reading of firmware 9.00's `libSceIdu.sprx` (module hash and reading in #21868), from disassembly and not run on a console; they are not checked against the 13.42 module. The reading gives: `sceIduIsAppInstalled(const char*)` requires a length of 36 or 9; `sceIduGetPkgInstallStatus` takes the length of its first argument before its other checks and requires 36; `sceIduNotificationShow` takes a 0x20-byte block whose `+0x00` must be `0x20` and whose string at `+0x10` must be non-empty, as must the string at `+0x08` when the signed 32-bit value at `+0x04` is 0 or less, and does not check `+0x18`; `sceIduTerm` takes no arguments and returns 0. The reading does not give the width of `+0x00`, read here as 32 bits like the size fields of the other exports. `sceIduTerm` releases nothing, since nothing is ever initialized here

## change 13 entries observed

- [Additional callable exports](../../../core/libs/prx/libSceIdu/Export.cpp) - 13 entries observed in a 3.20-labelled image and a later 13.60-labelled image. Neither image label authenticates a retail firmware version. Parameter lists give only what the checks of the two entries above read; return types and behavior past those checks are unverified. Unidentified names use literal NID aliases. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).
