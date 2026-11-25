# libSceAgcDriver

## change [ranges with a shared backing]

- On Linux, when the write-watch probe finds that host imports drop write tracking and dma-buf imports keep it and remain CPU-coherent, [ranges with a shared backing](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp) are imported through dma-buf only and a failed dma-buf import throws, except a range udmabuf refuses before importing anything (above its size limit, where the sparse path declines it up front), which keeps the host-pointer import and is compared instead of watched; a flexible memory range, which no other address maps, also keeps the host-pointer import after any failed dma-buf import; ranges without a shared backing (anonymous memory other than flexible memory, which has its own memfd) keep the host-pointer import, so FLAT/GLOBAL stores still reach them. A watched color target is refused instead of falling back to a host-pointer import that drops its write watch.
