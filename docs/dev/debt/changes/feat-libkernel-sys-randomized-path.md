# libkernel_sys

## remove | `__sys_randomized_path` |

## add Functional

- [`__sys_randomized_path`](../../../core/libs/prx/libkernel_sys/AdditionalExports.cpp) implements the read side of syscall 602: with `set == nullptr`, `*len` supplies capacity and receives the full sandbox-word length; `get` receives at most `capacity - 1` bytes plus NUL. A null `get` or `len` is a no-op. Invalid guest pointers return `-1`/`EFAULT` as a host-safety fallback. A non-null `set` still throws. The ABI follows [Obliteration](https://github.com/obhq/obliteration/blob/0831845b724f925b2d8cb546cfb5a630736508b3/legacy/src/process/mod.rs); [decanter](https://github.com/yerasuu/decanter/blob/15405de508234fc40b8bbbbed01c397c58e9e875/docs/missing-hle-spec.md) independently records the getter call. The fixed sandbox word is not per-process random. Setter, fault precedence and console/title behavior remain unmeasured.
