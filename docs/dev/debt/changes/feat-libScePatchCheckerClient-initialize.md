# libScePatchCheckerClient

## change [The uninitialized answers]

- [The uninitialized answers](../../../core/libs/prx/libScePatchCheckerClient/Export.cpp) (libScePatchCheckerClient) - 8 handler, check, request, cancel and query exports return `0x80182402`, only the 32-bit result being claimed, before initialization. In firmware 13.42's `libScePatchCheckerClient.sprx` (SHA-256 `7269b3c94359f86aabb6aaca5a7c87252d0b007e412e6c8fcfa64acf8b9e2447`), read from disassembly and not run on a console, each compares the pointer at `0xc018` with 0 before any argument is read or any call is made and returns `0x80182402` when it is null. It lies beyond its segment's file data, so it starts null; `scePatchCheckerInitialize` tracks initialization, returning `0x80182401` when already initialized, and `scePatchCheckerTerminate` clears it. The error names are unverified.
