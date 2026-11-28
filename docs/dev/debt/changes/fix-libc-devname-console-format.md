# libc

## change [devname and devname_r]

- [devname and devname_r](../../../core/libs/prx/libc/src/DeviceName.cpp) (libSceLibcInternal, NIDs `fqLrWSWcGHw` and `BIALMFTZ75I`) were measured on a PS5 payload process (firmware 11.60, probe `libc_devname`, build `b60e7ba4f34b`): the character device `0x1234` gets `#C:18:0x34`, the major number (bits 8 to 15) in decimal and the minor number (`device & 0xffff00ff`) in hexadecimal, and `errno` is 2 afterwards (the cause is not observed); `NODEV` and a block type give `#NODEV` with `errno` untouched, a short buffer truncates as `strlcpy` does, and a length of -1 writes the whole name with `errno` 14, here for every negative length. Other numbers follow the FreeBSD `major` and `minor` macros and block numbers get `#B:`, both unmeasured. Named devices (`null`, `zero`, `console` and `random` came back by name) are not modelled. The `devname` buffer is static and 64 bytes and `S_ISBLK(dev)` is tested, as in FreeBSD 11.4, not measured
