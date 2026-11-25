# libSceJsc

## add Functional

- [WTF::StringBuilder::resize](../../../core/libs/prx/libSceJsc/src/ApiBridge.cpp) (libSceJsc) - not compared with a console module; the console WTF version is unverified. libSceJsc exports it, but the builder storage (`StringImpl` buffers, `createSubstringSharingImpl`) is implemented only in libSceNKWebKit, which registers its `StringBuilder` resize at load on the 40-byte layout both libraries model; unregistered, a call throws. It follows pinned [StringBuilder.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WTF/wtf/text/StringBuilder.cpp#L70-L103): an overflowed builder is left unchanged, a buffer shared with another string is copied before the length drops, and a builder holding only a string keeps a substring of it. It throws where WebKit asserts or faults: a null builder and a size above the current length. Closes when: compared with a console module.

