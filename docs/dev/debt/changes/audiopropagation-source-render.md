# libSceAudioPropagation

## add Functional

- [sceAudioPropagationSourceRender](../../../core/libs/prx/libSceAudioPropagation/Export.cpp) renders only format 0, 1024-sample frames, PCM types 0 and 1 (int16 at 1/32767) and three-point paths with a material on the middle point; any other active path throws. Other formats and frame sizes, reflection order 2, rooms, portals and the `flags` bits are not measured. The model follows console impulse responses (firmware 13.00, `sceKernelGetSystemSwVersion` 13.590.001): `sceAudioPropagationSourceSetAudioPath` fixes the delay `(pathLength - |sourcePosition|) * 48000 / speed + 256`, plus one frame for a nonzero source position, the OpenAL rolloff and cone, and ACN/SN3D gains. The six measured 511-tap material bands drop taps beyond 255 samples (at most 6.6e-5). The console splits a fractional delay slightly differently (0.074 of the peak against 0.068 at 1979.07 samples).
