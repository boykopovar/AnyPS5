# libc

## add Functional

- [fgetws with a size below 2](../../../core/libs/prx/libc/src/FormattingWide.cpp) (libc) - leaves the stream unoriented and `errno` unchanged. The earlier code oriented the stream before the size check, following C11 7.21.2p4's wording for a wide input call. The system module of firmware 13.590.001, run in a payload process, returns null with `fwide` still 0 for sizes 0 and 1, and oriented the stream for size 2. The titles' copy, read from disassembly (see the fgetws entry), does the same. No title process was measured.
