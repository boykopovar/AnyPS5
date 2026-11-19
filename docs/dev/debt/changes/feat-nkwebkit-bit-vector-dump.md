# libSceNKWebKit

## change [WTF::BitVector]

- [WTF::BitVector](../../../core/libs/prx/libSceNKWebKit/src/WtfBitVector.cpp) (libSceNKWebKit) - `mergeSlow` ORs only the words both out-of-line vectors hold, where [BitVector.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/BitVector.cpp#L126-L142) reads past the other vector's words when this one is longer. `isEmptySlow`, `bitCountSlow` and `hashSlowCase` on an inline, empty or deleted vector, and a null vector anywhere, throw where WebKit reads through an invalid pointer; `equalsSlowCase` on two inline vectors compares their words. The one-word layout follows [BitVector.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/BitVector.h#L455-L509) for 64-bit pointers and is not measured on a console.

## add Functional

- [WTF::BitVector::dump](../../../core/libs/prx/libSceNKWebKit/src/WtfBitVectorDump.cpp) (libSceNKWebKit) - not compared with a console module. The body follows [BitVector.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WTF/wtf/BitVector.cpp#L262-L270) of August 2020, which matches the module exporting that revision's one-argument `resizeOutOfLine` and no `shiftRightByMultipleOf64`: each of the `size()` bits goes to the stream's virtual `vprintf` (vtable slot 2) as the format `1` or `-`, with no `begin`/`end` calls as in the 11.x `print`. A null vector, stream, vtable or `vprintf`, or an empty or deleted vector, throws; exceptions from `vprintf` propagate. Closes when: compared with a console module.

## remove `_ZNK3WTF9BitVector4dumpERNS_11PrintStreamE`
