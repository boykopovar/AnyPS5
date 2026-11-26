# libSceOpusCeltEnc

## add Functional

- [sceOpusCeltEncEncodeFloatWithPriority](../../../core/libs/prx/libSceOpusCeltEnc/Export.cpp) encodes synchronously through `libopus` (`EncodeFloat`), validating `data`, `maxDataBytes`, `priority` (20 to 100), and the frame size after the stack reservation check, and ignores `priority` for scheduling.

# libSceOpusSilkEnc

## add Functional

- [sceOpusSilkEncEncodeFloatWithPriority, sceOpusSilkEncEncodeWithPriorityAndTimeout and sceOpusSilkEncEncodeFloatWithPriorityAndTimeout](../../../core/libs/prx/libSceOpusSilkEnc/Export.cpp) encode synchronously through `libopus` for `EncodeFloatWithPriority` and for the infinite timeout (`0xFFFFFFFF`, without writing `batchId`) in the timeout variants; any finite timeout still throws through `NotImplemented_nid_no_patch` after the argument checks, and the validated priority (20 to 100) does not schedule anything.
