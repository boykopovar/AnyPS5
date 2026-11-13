# libSceIcu

## remove | `utext_setup` |

## change [ICU UText exports]

- [ICU UText exports](../../../core/libs/prx/libSceIcu/UText.cpp) are unverified on a console. `utext_setup` and `utext_close` follow ICU 68.2 [utext.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/utext.cpp#L578-L705), unchanged in ICU 78.1, with the 144-byte guest `UText` layout and guest-ABI provider callbacks. Allocation uses guest libc, not the stored `u_setMemoryFunctions` callbacks. Unlike upstream's no-op close, a non-null record without `UTEXT_MAGIC` still throws. Null status, an open text without a provider table, and allocation-size overflow throw where upstream dereferences or overflows. Other UText provider exports remain stubs. Closes when: compared with the console and remaining providers implemented.
