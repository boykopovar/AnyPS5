# libSceSdma technical debt

### Unknown function info

- [libSceSdma pending exports](../../../core/libs/prx/libSceSdma/Export.cpp) - sceSdmaCopyTiled, sceSdmaCopyTiledNonBlocking, sceSdmaCopyWindowT2T, sceSdmaCopyWindowT2TNonBlocking, sceSdmaCopyWindowTiled and sceSdmaCopyWindowTiledNonBlocking are listed in the [PS5 3.20 export table](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceSdma.c). Their signatures and behaviour are unverified; the declarations use the no-parameter unknown-signature convention and throw through `NotImplemented_nid_no_patch`.

- `_sceSdmaNop` — NID `vPP6vKHY8Tg` — its name and NID occur in the [PS5 3.20 libSceSdma import-stub list](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceSdma.c). The signature and behavior are unverified; the declaration in [Export.cpp](../../../core/libs/prx/libSceSdma/Export.cpp) uses the no-parameter unknown-signature convention and throws through `NotImplemented_nid_no_patch`, for import resolution only.

### Functional

- [libSceSdma](../../../core/libs/prx/libSceSdma/Export.cpp): the behaviour of Open, Close, the linear, fill and L2L window copies, fences and queue mode is not measured on a console. Copies run on the CPU when the queue is kicked, so a blocking call returns after they finish and the ring and fence area of the working buffer are never written. The order of source and destination and the fill size bit are read through AMD's public SDMA packet layouts. A fill with a pattern whose bytes differ throws in the default fill size and on unaligned ranges, and window fields outside their packet widths, commands larger than the ring and cancelling a queue of more than 65535 dwords throw: pending dwords are counted in 16 bits, so the ring checks see the wrapped count and a cancel would rewind only that many. Copies read guest memory directly, without waiting for GPU work that writes the source.
