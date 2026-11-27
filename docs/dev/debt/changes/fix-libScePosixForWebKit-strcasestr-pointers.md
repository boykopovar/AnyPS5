# libScePosixForWebKit

## change strcasestr

- [strcasestr](../../../core/libs/prx/libScePosixForWebKit/Export.cpp) (libScePosixForWebKit) - console locale behavior is unmeasured. It follows FreeBSD 11.4's [strcasestr.c](https://github.com/freebsd/freebsd-src/blob/releng/11.4/lib/libc/string/strcasestr.c) in the guest's supported C locale; other guest locales are not implemented. Both `needle` and `text` are read through unbounded page-by-page checked guest copies rather than direct dereferences, so a null pointer, an unreadable page encountered before a match or terminating NUL, or an allocation failure returns null instead of faulting the host. These refusals are host-safety fallbacks, unmeasured on a console.
