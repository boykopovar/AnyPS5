# libSceJsc

## remove | `_ZN3WTF25createCanonicalUUIDStringEv` |

## add Functional

- [WTF::createCanonicalUUIDString](../../../core/libs/prx/libSceJsc/src/WtfRandom.cpp) (libSceJsc) - the console return ABI and string allocation layout are unmeasured. The implementation follows [WebKit UUID.cpp at c55364d53f971f63a709976f168d932d558b8909](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WTF/wtf/UUID.cpp#L43-L60), returns its owned 8-bit `StringImpl` through the caller's hidden result pointer and uses the guest heap with inline characters at byte 20, as the existing WTF string implementation does. Null or unwritable return storage throws `std::invalid_argument`, where upstream would fault. Closes when: compared with a console module.
