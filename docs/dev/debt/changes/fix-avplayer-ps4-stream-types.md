# libSceAvPlayer

## change [sceAvPlayerGetStreamInfo / sceAvPlayerGetStreamInfoEx]

- [sceAvPlayerGetStreamInfo / sceAvPlayerGetStreamInfoEx](../../../core/libs/prx/libSceAvPlayer/src/Player.cpp) (libSceAvPlayer) - unverified on a console: whether some init path or SDK version reports other values, and the values of other stream types (timed text). No module or SDK header was used. In a PS5 process every player reports video as type 1 and audio as type 2, inferred from titles: the PPSA02433 logo players (`sceAvPlayerInit`, READY callback at app.elf 0x1509d50) and the PPSA02508 and PPSA03706 players (0x230-byte `sceAvPlayerInitEx`) enable type 2 as audio. In a PS4 process (`sceKernelIsPs4Process`) video is type 0 and audio type 1, the PS4 enum of [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/143b58aa48/src/core/libraries/avplayer/avplayer.h); CUSA00900's READY handler (eboot 0x2cff6a0) reads type 0 as video and type 1 as audio
