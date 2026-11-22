# libSceLibcInternal

## remove `H+T2VJ91dds`

## remove `s1EM2NdPf0Y`

## remove `XOgjMgZ3fjo`

## remove `m4kEqv7eGVY`

## remove `MQJQCxbLfM0`

## remove `ElKI+ReiehU`

## remove `VHBnRBxBg5E`

## remove `lzK3uL1rWJY`

## remove `XDm4jTtoEbo`

## remove `uiRALKOdAZk`

## add Functional

- [numpunct<wchar_t> construction and members](../../../core/libs/prx/libSceLibcInternal/Punctuation.cpp) (libSceLibcInternal) - follow the `libc.prx` bundled with PPSA21564, inspected statically and not run on a console: the constant grouping and names, and the separators, which are those of the C locale (the only locale `std::_Locinfo` holds) with `.` and `,` for a default facet. The separators do not come from `localeconv`, as the module's do. Narrow separators are widened by zero extension, not by `_Mbrtowc` with the stored conversion state; the constructors do not take the `_Locksyslock(0)` lock the module holds around `_Locinfo`; the `D2` destructors do not reinstall the base vtable; a null object or `_Locinfo` throws `std::invalid_argument` where the module would fault. Named construction accepts only the names `std::_Locinfo` accepts.

## remove `9biiDDejX3Q`

## remove `JTwt9OTgk1k`

## remove `GYTma8zq0NU`

## remove `cV6KpJiF0Ck`

## remove `ww3UUl317Ng`

## remove `HpAlvhNKb2Y`

## remove `VNaqectsZNs`

## remove `NeiFvKblpZM`

## remove `27Z-Cx1jbkU`

## remove `Zn44KZgJtWY`

## remove `pDFe-IgbTPg`

## remove `jerxcj2Xnbg`

## remove `n9-NJEULZ-0`

## add Functional

- [Stream runtime members](../../../core/libs/prx/libSceLibcInternal/StreamRuntime.cpp) (libSceLibcInternal) - follow the `libc.prx` bundled with PPSA21564, inspected statically and not run on a console. Null objects, null result strings and null vtables throw `std::invalid_argument` where the module would fault. The `_System_error_category` and `_Iostream_error_category` messages use the module's 78-entry table, which differs from this library's generic category table. `_Debug_message` prints `file(line) : error : message` and aborts.
- [basic_filebuf sync, pbackfail and _Endwrite](../../../core/libs/prx/libSceLibcInternal/StreamRuntime.cpp) (libSceLibcInternal) - follow the `libc.prx` bundled with PPSA21564, inspected statically, and read `basic_filebuf` at the module's offsets (get-area indirections at 0x18, 0x38 and 0x58, converter at 0x80, one-element cell at 0x88, mbstate at 0x90, FILE at 0xa8). Only objects laid out like the module's work: this library's `FileStream` lacks the module's `FILE` layout, so the module's `_Init` get area is not built here. Tested only on hand-built objects with fake codecvt and overflow vtables. `_Endwrite` keeps its shift sequence in a host vector, not a `std::string`. The other `basic_filebuf` members, `ios_base::clear`, `basic_ios::init` and the `ostream::sentry` members stay pending.

## remove `zugltxeIXM0`

## add Functional

- [std::_sceLibcLocinfo](../../../core/libs/prx/libSceLibcInternal/LocaleName.cpp) (libSceLibcInternal) - not run on a console; follows the `libc.prx` bundled with PPSA21564, inspected statically: an empty name or `C` returns and any other name, or a null one, raises `_Xruntime_error("bad locale name")`. A name the host cannot read raises the same error where the module would fault.

## remove | `_new_setup` | `KNNNbyRieqQ` |

## remove `9HILqEoh24E`

## add Functional

- [basic_string<char>::_Copy](../../../core/libs/prx/libSceLibcInternal/BasicString.cpp) (libSceLibcInternal) - retries a failed allocation at the requested size and throws `std::invalid_argument` for a kept length above the size or the new size, or a new size that fits the inline buffer, as the `wchar_t` `_Copy` here does; the body in the `libc.prx` bundled with PPSA21564, inspected statically, shows neither. The capacity rounding (to `| 15`, or by half the old capacity) and the 32-byte aligned large allocations follow that body.

## remove `SmtBNDda5qU`

## remove `G9LB1YD5-xc`

## remove `BNZq-mRvDS8`

## remove `wEU8oFtBXT8`

## remove `GGqIV4cjzzI`

## remove `k8zgjeBmpVY`

## remove `tCihLs4UJxQ`

## remove `zOPA5qnbW2U`

## remove `P0383AW3Y9A`

## remove `K-5mtupQZ4g`

## remove `6b3KIjPD33k`

## remove `bleKr8lOLr8`

## remove `aD-iqbVlHmQ`

## remove `So6gSmJMYDs`

## remove `pMWnITHysPc`

## remove `oi3kpQPqpMY`

## remove `b1LciG4lUUk`

## change yc-vi84-2aE

- [Additional 3.20 imports](../../../core/libs/prx/libSceLibcInternal/InventoryExports2.cpp) (libSceLibcInternal) - `_ZN6Dinkum7threads10lock_errorD0Ev` (NID `PG-2ZeVVWZE`); signatures and behavior are unverified and calls throw through `NotImplemented_nid_no_patch`.

## add Functional

- [Locale facet construction, _Getcat, locale and _Locinfo members](../../../core/libs/prx/libSceLibcInternal/FacetConstruction.cpp) (libSceLibcInternal) - the constructors, `_Init`, `_Getcat` of `collate`, `num_get`, `num_put<char>`, `money_get`, `money_put`, `time_get`, `time_put` and `messages`, `locale::_Locimp` construction and destruction, `locale::empty`, `locale::~locale`, the `_Locinfo` name constructors and `_Addcats`, the `ctype<wchar_t>` destructors, the `num_put<char>` and `money_put<char>` `_Put` and `_Rep` helpers, the `money_put<wchar_t>` `_Rep` helper and `Dinkum::codecvt::_Cvt_check` follow the `libc.prx` bundled with PPSA21564, inspected statically and not run on a console. The `_Locinfo` name constructors and `_Addcats` accept only the names `std::_sceLibcLocinfo` accepts and ignore the category. Null objects throw `std::invalid_argument` where the module would fault. `~_Locimp` releases every facet and deletes those whose count reaches zero, as the module does, but the module's `_Locimp::_Addfac` retains the new facet and releases the one it replaces and `_Addfac` here only stores the pointer, so a facet passed to `_Addfac` on a `locale::empty` implementation is released without having been retained. The `time_put<wchar_t>` `_Locinfo` constructors copy the C locale time names, not the names held by the `_Locinfo`; the two are the same while `_Locinfo` accepts only `"C"`.
- [num_get do_get for bool, wchar_t do_get for void* and _Getifld](../../../core/libs/prx/libSceLibcInternal/NumGet.cpp) (libSceLibcInternal) - follow the same `libc.prx`, inspected statically and tested on in-memory stream buffers only; a missing `numpunct` facet for `do_get(bool)` and a null `_Getifld` argument throw `std::invalid_argument` where the module would fault.
