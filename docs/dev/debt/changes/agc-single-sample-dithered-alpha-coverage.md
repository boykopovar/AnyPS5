# libSceAgcDriver

## change [Single-sample dithered alpha-to-coverage]

- [Single-sample dithered alpha-to-coverage](../../../core/libs/prx/libSceAgcDriver/Graphics/src/State.cpp) (AGC driver) - PS5/gfx1013 equivalence is unmeasured. The `0xe701`/`0x18701` masks follow full-rate gfx1032 RX 6650 XT register readbacks; RADV automatic flat VRS 2x2 produces a different spatial period. Raster/EQAA combinations other than one matched sample and one pixel iteration with `MSAA_ENABLE=0`, unmeasured alpha-mask words and MRT0 exports outside FP16_ABGR/32_ABGR remain unsupported. Request version 63 adds the single-sample codes; version 62 and older accepted layouts reject them. Closes when: compared with console masks and the remaining configurations are measured.
