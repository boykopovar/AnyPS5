# libSceNKWebKit

## add Unknown function info

- [WebCore::ExceptionData::isolatedCopy](../../../core/libs/prx/libSceNKWebKit/src/ExceptionData.cpp) (libSceNKWebKit) - not compared with a console module. The 16-byte record (4-byte `ExceptionCode` at offset 0, `WTF::String` at offset 8) and the copy follow [ExceptionData.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/dom/ExceptionData.cpp#L31-L38), the same at `e12a212a531a` and `73a35b43be7a`; the pinned 11.x header only has inline `const &` and `&&` overloads, which the inventory symbol does not match. The code is copied as stored. A null object throws. Closes when: compared with a console module.
