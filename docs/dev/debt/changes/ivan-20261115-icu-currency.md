# libSceIcu

## add Functional

- [Currency services](../../../core/libs/prx/libSceIcu/Currency.cpp) have no console ABI, data or cross-firmware comparison. The fifteen public `ucurr_*` adapters follow ICU 78.1 [ucurr.h](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/unicode/ucurr.h) and [ucurr.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/ucurr.cpp) with filtered `curr_tree` and `curr_supplemental` data. Localized strings and history can differ from firmware. Checked guest access rejects unreadable inputs and unwritable outputs with `NotImplemented_nid_no_patch`; these are host safety boundaries, not measured console errors. Closes when: ABI and results are compared with the console.

- [Historical currency dates](../../../core/libs/prx/libSceIcu/CurrencyBackend.cpp) extend the pinned source for valid data entries it crashes on: USN omits both dates and CNH omits `to`. Missing `from` uses ICU's minimum date; missing `to` is unbounded, following the availability resource reader. Explicit end dates remain exclusive. Raw CLDR fixtures and direct native calls for structurally safe entries establish the source/data contract; omitted-date behavior has not been compared with a console. Unexpected non-ISO historical codes throw. Closes when: missing-date behavior is measured on the console.

- [Currency object lifetime](../../../core/libs/prx/libSceIcu/Currency.cpp) inherits native ICU cleanup constraints: callers must stop use and close live enumerations before `u_cleanup`; registered keys and borrowed data cannot survive native cleanup. Guest allocator callbacks that throw or fault through native ICU frames remain unsupported. Currency registry calls are serialized with name copying, but concurrent cleanup versus other native ICU users is not qualified. Closes when: console lifetime behavior and exceptional allocator callbacks have a verified implementation.

## change [Enumeration dispatch]

- [Enumeration dispatch](../../../core/libs/prx/libSceIcu/Enumeration.cpp) is unverified on a console. `uenum_close`, `uenum_count`, `uenum_next` and `uenum_unext`, which firmware 13.42 exports ([#11013](https://github.com/boykopovar/AnyPS5/pull/11013#issuecomment-6401310351)), follow ICU 78.1 [uenum.cpp](https://github.com/unicode-org/icu/blob/release-78.1/icu4c/source/common/uenum.cpp) over the 56-byte `UEnumeration` of [uenumimp.h](https://github.com/unicode-org/icu/blob/release-78.1/icu4c/source/common/uenumimp.h), calling its callbacks with the guest ABI. `uprv_free` is the guest `free`, as ICU's default allocator is `free` and 13.42 exports no `u_setMemoryFunctions`. Currency enumeration uses these guest-ABI callbacks; the `uenum_reset` export remains unimplemented. A null status with a nonnull enumeration throws where ICU dereferences it. Closes when: compared with the console.

## remove | `ucurr_getName` |

## remove | `ucurr_openISOCurrencies` |
