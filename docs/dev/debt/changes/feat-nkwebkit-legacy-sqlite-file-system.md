# libSceNKWebKit

## add Functional

- [SQLiteFileSystem path, directory and modification time (3.20)](../../../core/libs/prx/libSceNKWebKit/src/LegacySQLiteFileSystem.cpp) (libSceNKWebKit) - `appendDatabaseFileNameToPath`, `deleteEmptyDatabaseDirectory` and `databaseModificationTime`, not compared with a console module. Only in the 3.20 inventory; they follow the 2020 [SQLiteFileSystem.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/sql/SQLiteFileSystem.cpp) and the `FileSystemPOSIX.cpp` functions it forwards to. The path join and the modification time (whole seconds of `stat`) are written out here, the 3.20 `WTF` functions not being exported by this library. Closes when: compared with a console module.
