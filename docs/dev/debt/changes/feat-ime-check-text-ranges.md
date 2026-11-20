# libSceIme

## change [sceImeCheckUpdateTextInfo, sceImeCheckRemoteEventParam]

- [sceImeCheckUpdateTextInfo, sceImeCheckRemoteEventParam](../../../core/libs/prx/libSceIme/Export.cpp) follow the checker at `0x3bc0` in the supplied `libSceIme.sprx` (SHA-256 `9664973a28c9de873cd74ca07c1631513ab34e56fcf2a9d4e930ac3c2506a80a`), read from disassembly. It checks up to `0x800` UTF-16 units, four areas, and modes 2 through 4. Invalid ranges return `0xfffffffd` with repair zero; nonzero repair resets invalid indices and caret, and bit 0 writes changes back. Remote events are checked only for id 1. Firmware logging is omitted. Unreadable guest structures or text throw; these cases were not measured on console.
