# libSceNgs2.native

## change [NGS2 mastering voice LFE, limiter and peak meter params]

- [NGS2 mastering voice LFE, limiter and peak meter params](../../../core/libs/prx/libSceNgs2.native/src/Voice.cpp) - peak meter validates its mode. The PS4 library throws for all three parameters. On PS5, an enabled LFE with the cutoff high bit set throws because the public parameter clamping for those words is unmeasured. Rendering the enabled filter with fewer than four channels throws. On firmware 13.590.001 the next render faults on one and three channels; two channels were not tested. Size 8 returns `0x804A8361`, sizes 16 and 24 are accepted, and peak meter modes above 2 return `0x804A8093`. PPSA02195 sends enabled effects. [PS5 measurements](https://github.com/boykopovar/AnyPS5/issues/8039#issuecomment-6792157201).

## add Functional

- [Mastering LFE filter](../../../core/libs/prx/libSceNgs2.native/src/Filter.cpp) - a fourth-order Chebyshev type I low-pass with 0.1 dB ripple approximates the firmware coefficient table. The implementation uses the system rate (the 96 kHz setting selects the 88.2 kHz design), filters voice channel 3 before limiting and output mapping, and clears state on setup. Bit-exact coefficients, effect order with remapped outputs or the limiter enabled, and event resets beyond setup remain unverified. [Firmware 13.42 unit measurements](https://github.com/boykopovar/AnyPS5/issues/21876#issuecomment-6948766633).
