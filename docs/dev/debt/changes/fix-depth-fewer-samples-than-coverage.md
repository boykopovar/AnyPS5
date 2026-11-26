# libSceAgcDriver

## add Functional

- [Depth targets with fewer samples than the rasterizer](../../../core/libs/prx/libSceAgcDriver/Graphics/src/State.cpp) (AGC driver) - a depth target with 1 < Z < N samples under an N-sample rasterizer (EQAA) is drawn at Z samples at the first Z coverage locations, only with `MSAA_ENABLE`, for draws that write no color target. Coverage samples past Z are dropped: the anchor association with `STATIC_ANCHOR_ASSOCIATIONS` 0 is not documented or measured, and a `DB_EQAA.MAX_ANCHOR_SAMPLES` above Z is assumed to change nothing. Pixels covered only by dropped samples run no pixel shader, so this rule and the sample-0 rule (Z = 1) refuse a pixel shader that writes memory. Alpha-to-coverage, `PS_ITER_SAMPLE`, centroid, sample, ancillary or coverage inputs, depth or sample-mask exports, overrasterization, and `DB_EQAA` Z interpolation with the depth test on throw. Closes when: measured on a PS5.
