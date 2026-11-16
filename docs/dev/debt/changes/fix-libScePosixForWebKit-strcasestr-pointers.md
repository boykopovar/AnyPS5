# libScePosixForWebKit

## change strcasestr

- [strcasestr](../../../core/libs/prx/libScePosixForWebKit/Export.cpp) (libScePosixForWebKit) - console locale behavior is unmeasured. It follows FreeBSD 11.4's [strcasestr.c](https://github.com/freebsd/freebsd-src/blob/releng/11.4/lib/libc/string/strcasestr.c) in the guest's supported C locale; other guest locales are not implemented. A null or unreadable `text` or `needle` previously faulted the host and now returns null, as does a string longer than 4096 bytes: both arguments pass through a bounded checked copy before the search. These refusals are host-safety fallbacks, unmeasured on a console.
