# libSceNKWebKit

## add Functional

- [WTF object dumps](../../../core/libs/prx/libSceNKWebKit/src/WtfObjectDump.cpp) (libSceNKWebKit) - not compared with a console module. The `MetaAllocatorHandle` layout (`m_start` at 40, `m_end` at 48) is derived from [MetaAllocatorHandle.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/MetaAllocatorHandle.h) and not measured. A null object or stream, a null `begin` result, and `BitVector::dump` on the hash-table values 0 and 1 throw where WebKit reads through an invalid pointer. The guest stream's vtable slots follow the existing PrintStream use.

## change [WTF::BitVector]

- [WTF::BitVector](../../../core/libs/prx/libSceNKWebKit/src/WtfBitVector.cpp) (libSceNKWebKit) - `mergeSlow` ORs only the words both out-of-line vectors hold, where [BitVector.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/BitVector.cpp#L126-L142) reads past the other vector's words when this one is longer. `isEmptySlow`, `bitCountSlow` and `hashSlowCase` on an inline, empty or deleted vector, and a null vector anywhere, throw where WebKit reads through an invalid pointer; `equalsSlowCase` on two inline vectors compares their words. `dump` is in [WTF object dumps](../../../core/libs/prx/libSceNKWebKit/src/WtfObjectDump.cpp). The one-word layout follows [BitVector.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/BitVector.h#L455-L509) for 64-bit pointers and is not measured on a console.

## remove `_ZNK3WTF19MetaAllocatorHandle4dumpERNS_11PrintStreamE`

## remove `_ZNK3WTF6Thread4dumpERNS_11PrintStreamE`

## remove `_ZNK3WTF9BitVector4dumpERNS_11PrintStreamE`
