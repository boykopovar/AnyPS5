# libSceNKWebKit

## add Unknown function info

- [WebCore::SharedBuffer mapped-file constructors](../../../core/libs/prx/libSceNKWebKit/src/SharedBuffer.cpp) (libSceNKWebKit) - not compared with a console module. The `SharedBuffer(FileSystem::MappedFileData&&)` C1 and C2 exports take the caller's mapping as the buffer's single `DataSegment` and set the size from it, as [SharedBuffer.cpp@73a35b43](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/SharedBuffer.cpp) does; an empty mapping still becomes one empty mapped segment. The buffer unmaps through the existing `MappedFileData` destructor export when the last segment reference drops, and the 32-bit size field caps a mapping. Null objects throw `std::invalid_argument`. Closes when: compared with a console module.
