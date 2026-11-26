# libSceAgcDriver

## add Functional

- [Color-writing draws over a depth target with fewer samples](../../../core/libs/prx/libSceAgcDriver/Graphics/src/State.cpp) (AGC driver) - a draw that writes color under an N-sample rasterizer over a depth target of 1 < Z < N samples is drawn at N samples against a copy of the depth target expanded to N samples: coverage sample i < Z reads stored sample i, and each later sample the stored sample nearest to it on the pixel's 1/16-pixel grid, the lower index on a tie. `DB_EQAA.STATIC_ANCHOR_ASSOCIATIONS` 0 is treated as static and a `MAX_ANCHOR_SAMPLES` above Z is ignored; neither rule nor the tie is measured. Only read-only depth and stencil are accepted: depth or depth-bounds tests, depth or stencil writes, occlusion queries, alpha-to-coverage, `PS_ITER_SAMPLE`, centroid, sample, ancillary or coverage inputs, depth or sample-mask exports, overrasterization and devices that put the guest's samples in order throw. Closes when: measured on a PS5.
