# libc

## add Functional

- [sprintf_s and vsprintf_s](../../../core/libs/prx/libc/src/Formatting.cpp) (libc) follow the `libc.prx` PPSA07923 and PPSA05916 ship: they share the secure conversions of `snprintf_s`, but every violation returns 0. A null buffer, a size of 0 or above `SIZE_MAX >> 3` reports "<name>: bad argument" and writes nothing; a null format "<name>: bad format argument"; output that does not fit stores `size - 1` characters, clears the first and reports "<name>: buffer overflow" with `ERANGE`; `%n` or a null string argument clears the first character and reports the `printf_s` message, followed by "<name>: buffer overflow" when the text before it already filled the buffer. The handler always gets a null pointer; the system `libSceLibcInternal` is not measured for these
