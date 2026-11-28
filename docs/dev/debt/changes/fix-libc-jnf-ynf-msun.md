# libc

## add Functional

- [jnf and ynf](../../../core/libs/prx/libc/src/Jnf.cpp) (libc) follow FreeBSD msun's [`e_jnf.c` at release/10.0.0](https://github.com/freebsd/freebsd-src/blob/release/10.0.0/lib/msun/src/e_jnf.c), calling this libc's msun `j0f`, `j1f`, `y0f`, `y1f` and `logf`. Console provenance, FMA and nonzero exceptions are unverified. `ynf(+-0)` preserves negative infinity without changing errno or exception flags. Finite negative arguments return a positive quiet NaN without errno or invalid, as `y0f`/`y1f` do; NaNs propagate first and negative infinity gives an invalid NaN. Order arithmetic wraps explicitly at 32 bits: `INT_MIN` skips recurrence, giving `j1f(abs(x))` or `y1f(x)` for finite nonzero x. Large orders can take billions of steps or fail to terminate, as upstream does. Closes when: the console float functions are measured or disassembled.
