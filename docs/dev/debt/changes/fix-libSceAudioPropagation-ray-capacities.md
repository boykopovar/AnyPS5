# libSceAudioPropagation

## add Unknown function info

- [Audio propagation ray getter capacities](../../../core/libs/prx/libSceAudioPropagation/Export.cpp) - the source and system getters currently report zero rays. The PS5 13.40 module reading recorded in [#20772](https://github.com/boykopovar/AnyPS5/pull/20772) indicates capacities of 6 for `sceAudioPropagationSourceGetRays` and `0x40` for `sceAudioPropagationSystemGetRays`. The capacities have not been independently measured on a console, and the host does not generate the corresponding ray directions.
