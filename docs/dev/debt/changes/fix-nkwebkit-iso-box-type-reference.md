# libSceNKWebKit

## add Functional

- [ISOBox::boxType return type](../../../core/libs/prx/libSceNKWebKit/src/IsoBoxAccessors.cpp) (libSceNKWebKit) - not compared with a console module. `boxType()` is in the 3.20 inventory only, where [ISOBox.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/graphics/iso/ISOBox.h#L51) declares it `const FourCC& boxType() const` (the header is the same on 2020-06-13 and 2020-11-12), so it returns the address of `m_boxType`, 16 bytes into the box, and not the 32-bit value that the pinned revision's `FourCC boxType() const` returns. Closes when: compared with a console module.
