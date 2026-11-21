# libSceNKWebKit

## add Functional

- [WebCore::TransformationMatrix inverse, scale3d, rotate3d(x, y, z, angle), blend, blend2 and blend4](../../../core/libs/prx/libSceNKWebKit/src/WebCoreTransform.cpp) (libSceNKWebKit) - are source-guided by pinned WebKit [TransformationMatrix.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f97/Source/WebCore/platform/graphics/transforms/TransformationMatrix.cpp#L852-L1775), a revision whose `blend` takes no `CompositeOperation` as the exported names require, not console-measured. `inverse` returns `WTF::Optional<TransformationMatrix>` as that revision's [Optional.h](https://github.com/WebKit/WebKit/blob/c55364d53f97/Source/WTF/wtf/Optional.h#L311-L316) lays it out, an engaged flag before the 16-byte-aligned matrix (144 bytes); the console module's layout is unverified. Products use the existing scalar `multiply`, not WebKit's x86-64 SSE2 path, whose rounding may differ.
