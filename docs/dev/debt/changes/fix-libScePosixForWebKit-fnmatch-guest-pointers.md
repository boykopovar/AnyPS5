# libScePosixForWebKit

## change [fnmatch]

- [fnmatch](../../../core/libs/prx/libScePosixForWebKit/Export.cpp) (libScePosixForWebKit) follows FreeBSD 11 [fnmatch.c](https://github.com/freebsd/freebsd-src/blob/stable/11/lib/libc/gen/fnmatch.c) in the C locale: each byte is one character, `FNM_CASEFOLD` folds ASCII letters only, and bracket ranges compare byte values. `[:class:]` names are not character classes: as in FreeBSD 11 through 14, `[[:digit:]]` is the set of `[`, `:`, `d`, `i`, `g`, `t` followed by a literal `]` (classes were added to `fnmatch.c` after 14). Multibyte characters and locale collation are not modelled. Both `pattern` and `string` are copied through unbounded page-by-page checked guest reads while preserving `errno`, so a null or unreadable pointer throws `std::invalid_argument` as a host-safety fallback unmeasured on a console.
