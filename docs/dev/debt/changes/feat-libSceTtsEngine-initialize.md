# libSceTtsEngine

## change [The uninitialized answers]

- [The uninitialized answers](../../../core/libs/prx/libSceTtsEngine/Export.cpp) (libSceTtsEngine) - only the 32-bit result is claimed. In firmware 13.42's `libSceTtsEngine.sprx` (SHA-256 `d022afb458a3754ab265d848e4101c496c04257cbaaf6717fb69ad22a1d6ee76`), read from disassembly and not run on a console, the other ten exports first test the engine pointer at `0x5ec1a0`, which starts zero (beyond the segment's file data) and is set only inside `sceTtsEngineInitialize` (`0x13c`). `sceTtsEngineInitialize` tracks initialization, returning `0x81A10003` when already initialized; with it null, `sceTtsEngineTerminate` returns 0 and the other nine return `0x81A10004` before reading any argument.
