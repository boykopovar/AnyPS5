# libSceIcu

## change [Unicode script properties]

- [Unicode script properties](../../../core/libs/prx/libSceIcu/Script.cpp): console ABI, data-version and title behavior are unverified. The ten queries use [ICU 78.1](https://github.com/unicode-org/icu/tree/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common) `uchar.cpp`, `propname.cpp`, `uscript_props.cpp` and `unicode/uscript.h` with Unicode 17.0 tables; the PS5 3.20 inventory NIDs do not establish parity with its ICU 68-era data. Invalid code points follow `uscript_getScript`'s implementation result -1, despite header prose saying 0. The three status/buffer APIs throw on unusable guest memory; null status returns immediately for script/extension queries but throws for sample queries. Closes when: compared with the console and its Unicode data version.

## add Functional

- [Script codes from names and locales](../../../core/libs/prx/libSceIcu/ScriptCode.cpp): an ID that is neither a script name nor a locale whose language or script ICU 68 maps directly throws `NotImplemented_nid_no_patch`, because ICU then needs likely subtags from locale data this build lacks. Console ABI and behavior are unverified. `uscript_getCode` follows ICU 68.2 [uscript.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uscript.cpp): script names resolve through ICU 78.1 property value names limited to the 193 script codes of ICU 68, and locale IDs through the ICU 78.1 `uloc_getLanguage` and `uloc_getScript` parsers. Closes when: compared with the console and likely-subtags data is available.
