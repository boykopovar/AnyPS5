# libSceLibcInternal

## add Unknown function info

- [inet6_option_space, inet6_option_init, inet6_option_append, inet6_option_alloc, inet6_option_next, inet6_option_find, inet6_opt_init, inet6_opt_append, inet6_opt_finish, inet6_opt_set_val, inet6_opt_next, inet6_opt_find and inet6_opt_get_val](../../../core/libs/prx/libSceLibcInternal/Ip6Options.cpp) (libSceLibcInternal) - follow FreeBSD 11.4's [`ip6opt.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/net/ip6opt.c) at the pinned commit (RFC 2292 and RFC 3542 ancillary-data options). The port writes into the caller's buffers directly, as FreeBSD does, and assumes the guest `cmsghdr` is FreeBSD's 12-byte header with 8-byte alignment for its data. The console's cmsghdr layout and the behavior for the same input are not measured.
