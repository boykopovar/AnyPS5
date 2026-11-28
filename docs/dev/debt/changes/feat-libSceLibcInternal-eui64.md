# libSceLibcInternal

## add Unknown function info

- [eui64_aton and eui64_ntoa](../../../core/libs/prx/libSceLibcInternal/Eui64.cpp) (libSceLibcInternal) - follow FreeBSD 11.4's [`eui64.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/net/eui64.c) at the pinned commit: `eui64_aton` accepts the canonical, Ethernet, classic and 48-bit MAC forms, and `eui64_ntoa` writes the dash-separated form, truncating it like `snprintf` into a buffer that is too short while still returning `-1`. The console's output for the same input is not measured. `eui64_ntohost` and `eui64_hostton` read `/etc/eui64` and are not provided.
