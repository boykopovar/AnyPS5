# libc

## add Functional

- [sscanf_s and vsscanf_s](../../../core/libs/prx/libc/src/Formatting.cpp) (libc), and plain `sscanf` and `vsscanf` formats scanned in process, follow the `libc.prx` of PPSA08194 and PPSA34008 for a floating field whose exponent is cut short: when `e` (or `p` after hex digits) and an optional sign are not followed by a digit within the width, the digits before it are converted and the marker and sign are consumed (`1e+x` stores 1 and counts 3, `%2lf` of `1E5` stores 1). Leading whitespace is consumed outside the field width. The stream forms, and plain Linux formats that glibc scans, keep the host result: glibc since its 2025 fix for bug 12701 treats these as a matching failure, earlier glibc consumes them as the console does
