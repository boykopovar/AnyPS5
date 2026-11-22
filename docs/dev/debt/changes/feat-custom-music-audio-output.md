# libSceCustomMusicAudioOut

## add Functional

- [sceCustomMusicAudioOutOutput and sceCustomMusicAudioOutGetLastOutputTime](../../../core/libs/prx/libSceCustomMusicAudioOut/Export.cpp) (libSceCustomMusicAudioOut) - route stereo PCM and its submission time through AudioOut based on the [firmware 13.42 readings](https://github.com/boykopovar/AnyPS5/issues/21885#issuecomment-6947728129) ([output and teardown](https://github.com/boykopovar/AnyPS5/issues/21885#issuecomment-6951125575)). Guest memory transfers are checked, and the host frame count is limited to 32768 because SDL may round the 16-bit count up to a power of two. The extra import `lTvit2qjY0o` and lifecycle helpers remain unmodeled. The all-music level is process-local until the system-setting imports are identified. Closes when: the missing imports and helpers are understood and the output path is compared with a console run.
