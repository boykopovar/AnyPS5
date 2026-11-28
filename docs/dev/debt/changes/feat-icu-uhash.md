# libSceIcu

## add Functional

- [Hash tables](../../../core/libs/prx/libSceIcu/HashTable.cpp) have no console ABI or behavior verification. Thirty-eight `uhash_*` exports take the signatures and layouts of ICU 68.2's [uhash.h](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uhash.h) and follow [uhash.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uhash.cpp): `uhash_puti` and `uhash_iputi` store a zero value, as before ICU 70, whose `uhash_putiAllowZero` the module does not export. `uhash_deleteHashtable` assumes ICU 68's `icu::Hashtable` layout. The four `UnicodeString` hash and comparison exports still throw. A null table, status, position, element, hash function or key comparator, an unknown resize policy and a full table throw where ICU dereferences, reads past its policy table or aborts. Closes when: compared with the console.
