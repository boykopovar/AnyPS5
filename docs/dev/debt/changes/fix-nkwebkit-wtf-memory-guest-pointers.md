# libSceNKWebKit

## change [FastBitVector exports]

- [FastBitVector exports](../../../core/libs/prx/libSceNKWebKit/src/WtfFastBitVector.cpp) have no console ABI or behavior verification. The 16-byte owner (word pointer at 0, 64-bit bit count at 8), range clearing, growth and copying follow the paired header and [PS5 11.x WebKit source](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/FastBitVector.cpp). `resizeSlow` stores the requested bit count together with the new words, which the inline upstream `resize` does right after calling it; unused bits of existing words are preserved and new words are zeroed. Allocation uses the module's `fastMalloc` and `fastFree`. Invalid ranges, null accessed storage, shrinking the word count and overflowing the upstream round-up throw instead of reproducing assertion failures or invalid memory access. Closes when: compared with a console module.
