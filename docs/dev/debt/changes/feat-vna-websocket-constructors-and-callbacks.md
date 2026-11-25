# libSceVnaWebsocket

## add Unknown function info

- [`MI6xIf-bHXs`, `K-logS9LtFw`, `qUkQKyPORE8` and `OUogBgTPp6E`](../../../core/libs/prx/libSceVnaWebsocket/Export.cpp) (libSceVnaWebsocket) - names and meanings are unknown; read from firmware 13.60 disassembly (`0x8e90`, `0x5c0`, `0x610`). `MI6xIf-bHXs`/`K-logS9LtFw` initialize a 44-byte config struct while leaving `[0x09..0x0f]` untouched; `qUkQKyPORE8`/`OUogBgTPp6E` zero seven 64-bit slots at `0x38..0x1b8` stride `0x40`. Null or unmapped pointers throw.
