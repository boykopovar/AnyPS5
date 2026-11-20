# libSceAgcDriver

## change [11_11_10 float CB resolve]

- [11_11_10 float CB resolve](../../../core/libs/prx/libSceAgcDriver/Graphics/src/MultisampleTarget.cpp) (libSceAgcDriver) - a CB resolve whose source is a multisampled 11_11_10 float color target sums the samples of each channel exactly, divides by the sample count and truncates, and returns the canonical NaN (0x7e0 for an 11-bit channel, 0x3f0 for the 10-bit one) when any sample of the channel is NaN, ahead of infinity. This follows the gfx1036 measurements posted in issue #11362; other RDNA generations and the console GPU itself are not measured. PPSA12361 reaches it with a two-sample 512x512 source (issue #11362).
