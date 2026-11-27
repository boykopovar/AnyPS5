# libc

## change [fwrite]

- [fwrite](../../../core/libs/prx/libc/src/FileAndHeap.cpp) (libc) - a `size` and `count` whose product overflows `size_t` no longer throws. If the wrapped product is 0, it returns 0 before the stream is read. If it wraps to a nonzero value, it returns 0 as a short write instead of reading past the caller's buffer. Firmware 13.42's `libSceLibcInternal.sprx` (`fwrite` at `0x6190`) multiplies with no overflow check and writes the wrapped byte count, so the wrapped-nonzero short write and wrapped-zero pre-stream-read behavior remain unmeasured console divergences (read from disassembly, not measured on a console).
