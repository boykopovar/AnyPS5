# libSceNKWebKit

## add Functional

- [ISOTrackEncryptionBox optional getters](../../../core/libs/prx/libSceNKWebKit/src/IsoBoxAccessors.cpp) (libSceNKWebKit) - not compared with a console module. `defaultCryptByteBlock` and `defaultSkipByteBlock` are in the 3.20 inventory only, whose WebKit is between 2020-06-13 and 2020-11-12; there they return `WTF::Optional<int8_t>` ([ISOTrackEncryptionBox.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/graphics/iso/ISOTrackEncryptionBox.h#L36-L37)), which is the flag byte then the value byte and comes back through a hidden pointer, an empty one as two zero bytes. The members are still read in the 11.x order the other accessors use, since only this library's own code would write them. Closes when: compared with a console module.
