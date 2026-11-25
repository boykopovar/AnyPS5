# libSceNKWebKit

## add Unknown function info

- [WTF file system link helpers](../../../core/libs/prx/libSceNKWebKit/src/WtfFileSystem.cpp) - not compared with a console module. `FileSystemImpl::hardLinkOrCopyFile` follows [FileSystem.cpp:730–742](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/FileSystem.cpp#L730-L742): it tries the guest libkernel_sys `link` export and, when linking fails, copies the target into an exclusively created link path, so a missing target and an existing link path both return false and the existing file is left untouched. The copy preserves the target's permission bits and removes a partially written link path; a hard-linkable target is never copied. Whether the console module falls back to a copy at all is unmeasured.

## remove `_ZN3WTF14FileSystemImpl18hardLinkOrCopyFileERKNS_6StringES3_`
