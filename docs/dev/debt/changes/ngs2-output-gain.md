# libSceNgs2.native

## change [NGS2 mastering voice gain]

- [NGS2 mastering voice gain](../../../core/libs/prx/libSceNgs2.native/src/Voice.cpp) (libSceNgs2.native) - the first gain a voice receives is flat, but every later one should ramp over the next grain in eight lanes, as measured against PS5 firmware 3.20 and 4.03; that ramp is not implemented. The standard mastering rack applies each gain after the mastering to render-buffer remap, per buffer channel, with the LFE level on buffer channel 3 whenever both channel counts are 4 or more. The PS4 library and custom mastering racks retain their previous per-source-channel gain placement, unverified against those modules. Measured: param 0x30000004 (id seen in PPSA22520) sets linear full-band and LFE factors, applied from the next render; each is clamped to [-32, 32] with NaN kept, and every value returns 0; a mastering setup resets both to 1, and STOP_IMM plus PLAY keeps them
