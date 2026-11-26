# libSceNgs2.native

## change [NGS2 patch destinations]

- [NGS2 patch destinations](../../../core/libs/prx/libSceNgs2.native/src/Voice.cpp) - a rack handle at input 0 selects its first voice (#4334). Multi-voice rack selection is unverified. Two-input custom submixers accept voice input 1. For standard native submixers, mapping patch input 1 to compressor buffer slot 1 is inferred from the [firmware's input-count and slot selection](https://github.com/boykopovar/AnyPS5/issues/15090#issuecomment-6643434499) and [PPSA35169's routing](https://github.com/boykopovar/AnyPS5/pull/15731). The PS4 library leaves input 1 unmixed. Nonzero rack-handle inputs, unsupported voice inputs, nonzero inputs on disconnected ports and routing changes during source render throw. GetPortInfo reports the resolved handle and input, with zero delay.

## change [Standard submixer compressor]

- [Standard submixer compressor](../../../core/libs/prx/libSceNgs2.native/src/Render.cpp) - console levels, module order, disabled pass-through and setup gain reset are unverified. It runs after mixing and refuses legacy UserFx, flags beyond `0x3f`, conflicting detector or link bits, and knees outside 0 to 2. The native `0x8` detector uses input 1 when available and input 0 otherwise. A [13.42 native run](https://github.com/boykopovar/AnyPS5/issues/21876#issuecomment-6950628991) confirmed the key controls gain on the program. The compressor curve follows the [unit measurements](https://github.com/boykopovar/AnyPS5/issues/15090#issuecomment-6643434499) and [gain and knee reading](https://github.com/boykopovar/AnyPS5/issues/15090#issuecomment-6672872206).
