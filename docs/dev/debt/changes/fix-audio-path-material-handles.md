# libSceAudioPropagation

## change [sceAudioPropagationSourceSetAudioPath]

- [sceAudioPropagationSourceSetAudioPath](../../../core/libs/prx/libSceAudioPropagation/Export.cpp): active nonempty acoustic path rendering remains unimplemented. The point handle at offset 16 names a registered material, as declared by [SharpProspero](https://github.com/SvenGDK/SharpProspero/blob/5dfdb4b0b166e83dc9ae275001afb695f12c7174/src/SharpProspero/Interop/Audio/AudioPropagation.cs#L271-L287) and observed in SAROS. The host checks kind and system ownership; native errors and validation order remain unmeasured. Paths copy 224 bytes with four 48-byte points and opaque trailing eight-byte fields. The 36 initially empty, invalid slots and allocation before calculation remain inferred. Source/system destruction invalidates owned slots.
