# libc

## change [`freopen` of a closed stream]

- [`freopen` of a closed stream](../../../core/libs/prx/libc/include/FileStream.hpp) (libc) opens the file on a new host descriptor, so `fileno` after `fclose(stdout)` and `freopen(path, mode, stdout)` is whatever the host hands out, where a PS5 takes the lowest free descriptor. The stream object is kept and reopened as there, and a missing path (`ENOENT`), an overlong one (`ENAMETOOLONG`) and a null filename (`EBADF`) answer as measured on a console. On the closed stream `fputs` and `fputc` (which `putc`, `putchar` and `__swbuf` forward to) return -1 leaving `errno` unchanged and `fprintf` gives `EINVAL`, as measured on the console; the other writes, unmeasured, answer `EBADF`.
