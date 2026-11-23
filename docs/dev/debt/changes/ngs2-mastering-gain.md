# libSceNgs2.native

## change [NGS2 mastering voice gain]

- [NGS2 mastering voice gain](../../../core/libs/prx/libSceNgs2.native/src/Voice.cpp) (libSceNgs2.native) - firmware [3.20 and 4.03](https://github.com/boykopovar/AnyPS5/pull/10410) measured first gain flat and later gains ramping in eight lanes over one grain. The host uses the [port ramp's lane arithmetic](https://github.com/boykopovar/AnyPS5/pull/13703); exact mastering rounding, transitions through NaN, control coalescing and silent-grain history are unverified. Standard mastering applies gain after output remap; output channel 3 gets LFE gain when both counts are at least 4. PS4 and custom mastering use their unverified per-source gain placement and flat gain. Param 0x30000004 (seen in PPSA22520) accepts all values, clamps each linear factor to [-32, 32] and keeps NaN. Setup resets both to 1; STOP_IMM plus PLAY preserves them.
