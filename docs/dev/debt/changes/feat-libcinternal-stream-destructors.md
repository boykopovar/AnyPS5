# libSceLibcInternal

## remove `+Uuj++A+I14`

## remove `QJJ-4Dgm8YQ`

## remove `kvqg376KsJo`

## remove `q05IXuNA2NE`

## remove `0j1jspKbuFk`

## remove `HSkPyRyFFHQ`

## remove `RP7ijkGGx50`

## remove `7VRfkz22vPk`

## remove `EYZJsnX58DE`

## remove [_ZNSt13basic_istreamIwSt11char_traitsIwEED0Ev]

## remove [_ZTv0_n24_NSiD0Ev]

## remove [_ZTv0_n24_NSt13basic_istreamIwSt11char_traitsIwEED0Ev]

## remove [_ZNSiD0Ev]

## remove [_ZNSiD1Ev]

## remove [_ZNSoD0Ev]

## remove [_ZNSoD1Ev]

## add Functional

- [basic_istream and basic_ostream destructors](../../../core/libs/prx/libSceLibcInternal/StreamDestructors.cpp) (libSceLibcInternal) - the console's `libSceLibcInternal.sprx` was not read and no title was seen calling them. The `D1` and `D0` destructors of both streams for `char` and `wchar_t` and their `_ZTv0_n24_` thunks have the limits of the `libc` `_ZNSt8ios_baseD2Ev` export they run on the `basic_ios` virtual base. They find that base, and a thunk its complete stream, through the offset 24 bytes before the vtable's address point, where the console may use a fixed offset; they leave the vtable pointers as they were, and a null or unreadable object or vtable throws `std::invalid_argument`, where the console faults
