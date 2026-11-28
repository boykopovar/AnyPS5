# libc

## change [fwrite]

- [fwrite](../../../core/libs/prx/libc/src/FileAndHeap.cpp) (libc) - a `size` and `count` whose product overflows `size_t` no longer throws. If the wrapped product is 0, it returns 0 before the stream is read, matching the existing zero-size path. If it wraps to a nonzero value, it returns 0 as an unsignaled short write instead of passing the wrapped byte count to `std::fwrite`. Firmware 4.03 `libc.prx` (`fwrite` at `0x6190`) multiplies with no overflow check and writes the wrapped byte count, so the wrapped-nonzero rejection is a deliberate source-level divergence; no console measurement (read from disassembly, not measured on a console).
