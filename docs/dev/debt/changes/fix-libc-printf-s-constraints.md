# libc

## change [fprintf_s, vfprintf_s, vprintf_s, fwprintf_s, wprintf_s, vfwprintf_s and vwprintf_s]

- [fprintf_s, vfprintf_s, vprintf_s, printf_s, fwprintf_s, wprintf_s, vfwprintf_s and vwprintf_s](../../../core/libs/prx/libc/src/Formatting.cpp) (libc) follow C11 K.3.5.3.1, K.3.5.3.2, K.3.5.3.10 and K.3.9.1.1 with the constraints of `snprintf_s` and `swprintf_s`: a null stream, format or argument list, any `%n`, and a null `%s`, `%ls` or `%S` argument return -1 and write nothing, because the output is formatted completely before it is written. A violation only returns: the constraint handler is not invoked, `errno` is untouched, and the value is the one the narrow `snprintf_s` returns on this branch, not a result measured for a stream function on a PS5. A closed stream sets `EBADF` (9) as `fprintf` does. `printf_s` and `vprintf_s` write to the guest standard output stream, and the wide forms count characters while the narrow ones count bytes, as their unchecked siblings do
