# libSceNKWebKit

## add Functional

- [WTF::FileSystemImpl::hardLinkOrCopyFile](../../../core/libs/prx/libSceNKWebKit/src/WtfFileSystemCopy.cpp) - console ABI and filesystem error behavior are unverified. The hard-link attempt and non-overwriting file-copy fallback follow [PS5 WebKit FileSystem.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/FileSystem.cpp) at `80f77e94c63ee7202eec3d4b5b1a021dd89011ba`, using the host filesystem after guest path resolution; host permissions and symbolic-link behavior can differ from the console. Archive-only source files remain subject to the path resolver's unsupported-operation exception.

## remove | `_ZN3WTF14FileSystemImpl18hardLinkOrCopyFileERKNS_6StringES3_`
