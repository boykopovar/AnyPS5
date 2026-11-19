# libSceNgs2.native

## change [NGS2 mastering voice LFE, limiter and peak meter params]

- [NGS2 mastering voice LFE, limiter and peak meter params](../../../core/libs/prx/libSceNgs2.native/src/Voice.cpp) - enabled LFE throws. Firmware uses one of two fixed channel-3 IIR low-passes selected by `fc` (80 or below, 81 or above). Two-channel behavior is unverified. Peak meter control validates mode. The PS4 library throws for all three parameters. [PS5 measurements on 13.590.001](https://github.com/boykopovar/AnyPS5/issues/8039#issuecomment-6792157201) found that size 8 returns `0x804A8361`, sizes 16 and 24 are accepted, and peak meter modes above 2 return `0x804A8093`. PPSA02195 sends enabled effects.

## add Functional

- [Mastering limiter rendering](../../../core/libs/prx/libSceNgs2.native/src/Render.cpp) - the implementation uses the render system's sample rate, runs before channel mapping and output gain, and resets on voice setup and rendered bypass. Firmware placement, rate source, event lifecycle flags and bit-exact output remain unverified. [Firmware 13.42 disassembly and DSP harness results](https://github.com/boykopovar/AnyPS5/issues/21876#issuecomment-6950628991) establish the linked peak detector, 32-sample blocks, and 10 ms attack and 100 ms release.
