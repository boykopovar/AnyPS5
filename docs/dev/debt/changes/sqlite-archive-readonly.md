# libSceNKWebKitRequirements

## change Databases that exist only inside `app0.zar`

- [`sqlite3_open_v2`](../../../core/libs/prx/libSceNKWebKitRequirements/src/Sqlite.cpp) ignores the guest VFS argument. `file:` URIs, whether passed to the export with `SQLITE_OPEN_URI` or evaluated by SQL, see only the wrapped host default VFS and its memory-only `memdb` VFS. Other native VFS names are unavailable. Database files inside `app0.zar` open read-only. An archive-only database with a nonempty rollback journal or WAL in the archive or loose directory is refused because snapshot reads cannot perform journal recovery. SQLite uses host sync calls rather than the guest file layer. Read-write opens record the database and its `-wal`, `-shm` and `-journal` paths for `sceKernelSync`.
