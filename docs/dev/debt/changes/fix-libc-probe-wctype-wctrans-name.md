# libc

## add Unknown function info

- [wctype and wctrans names](../../../core/libs/prx/libc/src/RuneLocale.cpp) (libc) - the property name is copied in with `CopyGuestString_nid_no_patch` capped one byte past the longest known name (10 for `wctype`, 8 for `wctrans`), so longer names still match nothing; an unreadable name returns 0 like an unknown one, because the console's behaviour on unmapped memory is unmeasured.
