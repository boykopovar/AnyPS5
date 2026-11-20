# libc

## add Functional

- [ungetwc](../../../core/libs/prx/libc/src/StandardStreams.cpp) (libc) - the bytes it pushes back are not measured on a console. Like FreeBSD 11.4's [`__ungetwc`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/libc/stdio/ungetwc.c#L50-L68), which pushes back the bytes `wcrtomb` gives, it hands the host `ungetc` the UTF-8 bytes that `fgetwc` reads, last byte first; a high surrogate pushed back while `fgetwc` holds the low surrogate after it goes back with it as their four-byte form. On Windows, where the C runtime keeps one pushed-back character, a non-ASCII character can therefore fail after part of it was pushed back. A value outside 0 to 0xFFFF other than `WEOF` fails with `EILSEQ` (86) and sets the error indicator, also unmeasured; FreeBSD's code does not reject it, and with the console's 16-bit `wchar_t` it would be truncated by `wcrtomb` instead.
