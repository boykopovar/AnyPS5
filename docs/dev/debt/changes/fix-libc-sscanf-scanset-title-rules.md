# libc

## add Functional

- [Narrow stream scansets](../../../core/libs/prx/libc/src/Formatting.cpp) (libc): on Linux and macOS, `fscanf`, `vfscanf`, `scanf`, `vscanf` and their `_s` forms hand narrow scansets to the host scanner: its range rules apply, not the titles' (measured with `sscanf` and `sscanf_s` only), and a negated set reads a NUL byte. On Linux, `fscanf` formats without a narrow `%c` use glibc

## change [Narrow scansets, string lengths and unknown conversions]

- [Narrow scansets, string lengths and unknown conversions](../../../core/libs/prx/libc/include/WindowsScanning.hpp) (libc): `fscanf` formats that glibc scans store `%lls` and `%Ls` as 32-bit host wide units where the bundled libraries store bytes; plain `%s` at end of input can clear errno. The shared secure scanner stops unknown conversions before testing input EOF. A format byte from 0x80 up outside a scanset matches its input byte here, where the `libc.prx` of PPSA07923 and PPSA05916 fails the match. In `sscanf`, `vsscanf` and their `_s` forms, and in every form on Windows, scansets follow those titles: `X-Y` is a range when a `-` follows a member and is not last, a reversed range matches nothing, a `-` first, last or after a range is a member, a `]` that opens the set can start a range, and an unterminated set ends the scan. `%s`, `%c` and `%[` read narrow with any length but `l`, and an unknown conversion such as `%y` ends the scan as a matching failure, also in the `_s` forms
