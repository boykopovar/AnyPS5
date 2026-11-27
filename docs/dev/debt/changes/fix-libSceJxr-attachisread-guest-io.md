# libSceJxr

## change [attachISRead]

- [attachISRead](../../../core/libs/prx/libSceJxr/src/BitIo.cpp#L75): not measured on a PS5: guest layout and callback behavior remain unverified. It follows pinned jxrlib [strcodec.c](https://github.com/4creators/jxrlib/blob/f7521879862b9085318e814c6157490dd9dbbdb4/image/sys/strcodec.c#L1065-L1088): get position into the original offset, point start/current at `io - 8192`, set position, read 8192 bytes, advance the unsigned offset, load the first four bytes big-endian, clear bits used and set the read mask and stream. Callback return values are ignored; `pSC` is unused with `DISABLE_PERF_MEASUREMENT`. Null state, stream or callback throws, and an unwritable bit state throws before any field write.
