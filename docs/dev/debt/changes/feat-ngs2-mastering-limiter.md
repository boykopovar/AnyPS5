# libSceNgs2.native

## add Functional

- [Mastering limiter rendering](../../../core/libs/prx/libSceNgs2.native/src/Render.cpp) - the implementation uses the render system's sample rate, runs before channel mapping and output gain, and resets on voice setup and rendered bypass. Firmware placement, rate source, event lifecycle flags and bit-exact output remain unverified. [Firmware 13.42 disassembly and DSP harness results](https://github.com/boykopovar/AnyPS5/issues/21876#issuecomment-6950628991) establish the linked peak detector, 32-sample blocks, and 10 ms attack and 100 ms release.
