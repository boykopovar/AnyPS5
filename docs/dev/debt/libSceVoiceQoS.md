# libSceVoiceQoS technical debt

### Unknown function info

- [sceVoiceQoSSetConnectionAttribute](../../../core/libs/prx/libSceVoiceQoS/Export.cpp) (libSceVoiceQoS, NID `KCGOxg8iX7s`) is a throwing stub declared without parameters, like the other endpoint calls of the library: PPSA04159 fails to load without it and its signature is unknown. The name hashes to the imported NID (`tools/nid_names.py`)
- [sceVoiceQoSInit](../../../core/libs/prx/libSceVoiceQoS/Export.cpp) (libSceVoiceQoS) - `0x804E0905` for a second initialization comes from [prosper](https://github.com/mattias800/prosper)'s reading of the module and was not measured: on a PS5 a valid call blocks inside a homebrew payload, so it never returns there. PPSA26344's two call sites (`0xE307BF`, `0x151C45D`) pass an allocated pointer, a 0x40000-byte block and app type 0x10000000; the app type and block are not used, as no endpoint is implemented

### Functional

- [sceVoiceQoSEnd](../../../core/libs/prx/libSceVoiceQoS/Export.cpp) (libSceVoiceQoS) - `0x804E0901` (not initialized) before any initialization was measured on a PS5 (firmware 13.60, #1407); ending an initialized library was not, because a valid `sceVoiceQoSInit` never returns inside a homebrew payload. PPSA14632 calls it at shutdown and PPSA26344 after a nonzero `sceVoiceQoSInit`. After an initialization it returns 0 and clears the init flag, so the library can be initialized again. Whether the console also ends the libSceVoice instance that its `sceVoiceQoSInit` starts ([prosper](https://github.com/mattias800/prosper)'s reading of the module), and what it answers while endpoints exist, is unknown; no endpoint is implemented
