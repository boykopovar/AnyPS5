# libSceJsc

## add Functional

- [WTF::FileSystemImpl::pathByAppendingComponent](../../../core/libs/prx/libSceJsc/src/WtfFileSystem.cpp) (libSceJsc) - not compared with a console module; the console WTF version is unverified. Follows pinned [FileSystemPOSIX.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f97/Source/WTF/wtf/posix/FileSystemPOSIX.cpp#L316-L321): `path + component` when the path ends in `/`, else `path + "/" + component`, as [tryMakeStringFromAdapters](https://github.com/WebKit/WebKit/blob/c55364d53f97/Source/WTF/wtf/text/StringConcatenate.h#L387-L415) builds it: a new internal-buffer string, 8-bit unless either input is 16-bit, a null string counting as empty. The result is returned through the hidden `String` pointer. Where WTF crashes, on a result over `String::MaxLength` or a failed allocation, this throws; it also throws without the registered libSceNKWebKit `fastMalloc`. Closes when: compared with a console module.
