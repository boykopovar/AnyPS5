# libSceNKWebKit

## add Unknown function info

- [WTF file system helpers](../../../core/libs/prx/libSceNKWebKit/src/WtfFileSystem.cpp) - not compared with a console module. They follow the `std::filesystem` branch of [FileSystem.cpp:281–295 and 611–870](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/FileSystem.cpp#L281-L295), `getFileDeviceId` in [FileSystemPOSIX.cpp:284–295](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/posix/FileSystemPOSIX.cpp#L284-L295) and [FileSystemPlayStation.cpp:60–71 and 139–145](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/playstation/FileSystemPlayStation.cpp#L60-L145), using the guest libkernel `stat`, `lstat`, `unlink` and `rmdir`, the guest libc `realpath` and `RenameGuestPath_nid_no_patch`, the guest `rename` body. `realPath` uses a 1024-byte `PATH_MAX` buffer.

## remove `_ZN3WTF14FileSystemImpl10deleteFileERKNS_6StringE`

## remove `_ZN3WTF14FileSystemImpl10fileExistsERKNS_6StringE`

## remove `_ZN3WTF14FileSystemImpl19filesHaveSameVolumeERKNS_6StringES3_`

## remove `_ZN3WTF14FileSystemImpl20deleteEmptyDirectoryERKNS_6StringE`

## remove `_ZN3WTF14FileSystemImpl8moveFileERKNS_6StringES3_`

## remove `_ZN3WTF14FileSystemImpl8realPathERKNS_6StringE`
