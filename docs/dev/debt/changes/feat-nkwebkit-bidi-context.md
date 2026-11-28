# libSceNKWebKit

## add Functional

- [WebCore::BidiContext](../../../core/libs/prx/libSceNKWebKit/src/BidiContext.cpp) (libSceNKWebKit) - `create` and `copyStackRemovingUnicodeEmbeddingContexts`, not compared with a console module. They follow `BidiContext.cpp` of the 11.x sources. The object has the layout of the 11.x header for the caller's inline accessors (16 bytes: the reference count, one word holding the level on 6 bits, the direction on 5, the override flag and the source, then the parent), derived, not measured. The four parentless contexts are created once and shared, as in the source. A stack whose contexts all come from Unicode throws `std::invalid_argument` where the source reads past an empty vector, and null arguments throw it where the source dereferences them. Closes when: compared with a console module.

## remove `_ZN7WebCore11BidiContext41copyStackRemovingUnicodeEmbeddingContextsEv`

## remove `_ZN7WebCore11BidiContext6createEh14UCharDirectionbNS_19BidiEmbeddingSourceEPS0_`
