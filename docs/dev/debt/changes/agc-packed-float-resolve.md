# libSceAgcDriver

## change [11_11_10 float CB resolve]

- [11_11_10 float CB resolve](../../../core/libs/prx/libSceAgcDriver/Graphics/src/MultisampleTarget.cpp) (libSceAgcDriver) - a CB resolve whose source is a multisampled 11_11_10 float color target averages the unpacked samples of each channel in 32-bit float and stores the average truncated toward zero, as the target's ROUND_MODE does for its own stores. The console's averaging order, intermediate precision and rounding for that resolve are not measured and no known source describes them. PPSA12361 reaches it with a two-sample 512x512 source (issue #11362).
