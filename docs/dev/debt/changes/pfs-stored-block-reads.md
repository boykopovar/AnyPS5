# libScePfs

## change [scePfsPread, scePfsPreadDecompressBlock]

- [scePfsPread, scePfsPreadDecompressBlock](../../../core/libs/prx/libScePfs/Export.cpp) (libScePfs) - compressed blocks and v3 shuffled layouts still throw. Stored v2 halves and v3 plain blocks were compared with the PPSA28050 module on Linux. A positive direct read starting at or beyond the end of a stored v2 final half returns out of range as a host bounds check. Other module revisions, physical-console behavior, partially written scratch buffers after errors and invalid guest spans have not been compared.
