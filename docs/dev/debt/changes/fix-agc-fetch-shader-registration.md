# libSceAgcDriver

## change [Registered shader types]

- [Registered shader types](../../../core/libs/prx/libSceAgcDriver/Execution/src/Driver/Shaders/ShaderRegistry.cpp) (libSceAgcDriver) - types 0, 1, 2, 4, 6 and 7 are prepared at registration. Types 3, 5 and 8 (`Hs`, `HsFront` and `Fs` in libSceAgc's `ShaderBinaryType`) are skipped there without a message; type 8 is a fetch shader, which runs inside the vertex program that calls it. Any other type is logged (`unsupported registered shader type N; it is not prepared at registration`), left unprepared at registration, and throws when it is used later. Hello Neighbor 2 (PPSA07426), Ad Infinitum (PPSA10600), Ghostrunner (PPSA03685) and Forspoken (PPSA03984, about 2,900 headers per run) register type 8 headers and run on past them; #1892 found no context registers in one.
