# libSceLibcInternal

## add Unknown function info

- [ether_aton and ether_ntoa](../../../core/libs/prx/libSceLibcInternal/Ether.cpp) (libSceLibcInternal) - follows FreeBSD 11.4's [`ether_addr.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/net/ether_addr.c) at the pinned commit: `ether_aton` parses six hexadecimal octets and returns null unless `sscanf` reads exactly six, storing each value in one byte; `ether_ntoa` formats two-digit lowercase hex into a static 18-byte buffer. A null argument returns null, where FreeBSD would fault. The console's output for the same input is not measured.
