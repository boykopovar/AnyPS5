# libc

## add Functional

- [setvbuf sizes](../../../core/libs/prx/libc/src/StandardStreams.cpp) (libc) - in the buffered modes a size whose `int` value is negative returns `EOF` without touching the stream, as FreeBSD 11.4's [`setvbuf`](https://github.com/freebsd/freebsd-src/blob/d106002967eff712894ab8b5337bbbff106ce5e0/lib/libc/stdio/setvbuf.c#L57-L64) does; not measured on a console. On Windows the stream's buffer is the host C library's, which accepts sizes from 2 to `INT_MAX` only: a size of 1 asks the host for 2, and a size above `INT_MAX` whose `int` value is not negative (`0x100000000` and up) throws `std::length_error`. Closes when: `setvbuf` is measured on a console with these sizes.
