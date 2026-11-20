# libc

## change [fwscanf, vfwscanf, wscanf, vwscanf and their `_s` forms]

- [fwscanf, vfwscanf, wscanf, vwscanf and their `_s` forms](../../../core/libs/prx/libc/src/ScanningWide.cpp) (libc) run the `swscanf` scanner on characters read with `fgetwc`, so everything listed for `swscanf` in this file applies to them. As in the console's `libc.prx`, a field takes characters while they can still begin a valid item, so a failed field such as `0x` for `%x` stays consumed, and only the character that ended the scan goes back to the stream. That character is pushed back as its UTF-8 bytes with `ungetc`, because `ungetwc` takes single bytes only; the Windows C runtime guarantees one byte of push-back, so a non-ASCII lookahead can be lost there. A high surrogate goes back with the low surrogate `fgetwc` held for it, as the four bytes of the character. A null stream or format returns `EOF`; the console checks them only in the `_s` forms.
