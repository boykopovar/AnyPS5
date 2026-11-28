# libSceAgc

## change [sceAgcFuseShaderHalves_0200 embedded constant buffer]

- [sceAgcFuseShaderHalves_0200 embedded constant buffer](../../../core/libs/prx/libSceAgc/Misc/src/ShaderFusion.cpp) (libSceAgc) - front halves with an unrecognized `s_getpc_b64` use, an offset between the program's end and the embedded buffer, or a buffer running into the trailer throw. The front constant buffer (at its trailer's code size, PPSA04921) is moved after the copied back code, and each `s_getpc_b64` / `s_add_u32` literal / `s_addc_u32 0` offset into it is moved with it. On the console the code stays in place, so the offsets need no change; not measured.
