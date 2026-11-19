# libSceNKWebKit

## add Functional

- [WebCore::SQLiteFileSystem::ensureDatabaseDirectoryExists](../../../core/libs/prx/libSceNKWebKit/src/SQLiteFileSystem.cpp) (libSceNKWebKit) - not compared with a console module. A null or empty path returns false; any other path goes to the existing `FileSystemImpl::makeAllDirectories`, as the pinned [PS5 WebKit 11.x SQLiteFileSystem.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/sql/SQLiteFileSystem.cpp#L61-L66) does.

## remove | `_ZN7WebCore16SQLiteFileSystem29ensureDatabaseDirectoryExistsERKN3WTF6StringE` | `MRGoMKt8Wr0` |
