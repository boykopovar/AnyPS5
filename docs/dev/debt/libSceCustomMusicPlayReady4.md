# libSceCustomMusicPlayReady4 technical debt

### Unknown function info

- [Callable exports](../../../core/libs/prx/libSceCustomMusicPlayReady4/Export.cpp): these exports have unverified signatures and behavior. Some are named after the symbols the [verified NID name database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) gives for their NIDs, and the NID of each name matches; the others are unidentified. These are throwing import-resolution placeholders; no playback functionality is implemented. No matching NIDs were found in the later module inventory. Data exports require separate layout and ownership analysis.
