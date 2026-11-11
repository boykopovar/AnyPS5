# libSceNKWebKitRequirements technical debt

### Known export names; ABI prototypes unverified

- 11 pending exports in [Export.cpp](../../../core/libs/prx/libSceNKWebKitRequirements/Export.cpp) are present in the [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs) inventory. Their prototypes and behavior are unverified. They use the no-parameter import-resolution convention and throw through `NotImplemented_nid_no_patch`; these declarations make no ABI or behavior claim.
- None of them is declared in [Identified.hpp](../../../core/libs/prx/libSceNKWebKitRequirements/include/Identified.hpp); the 11 hash to no identifier of libjpeg-turbo 3.0.4, libpsl 0.21.5, libxml2 2.13.4, zlib 1.3.1, SQLite 3.46.1, ICU 76.1, HarfBuzz 10.0.1, cairo 1.18.2, pixman 0.42.2, FreeType 2.13.3, libpng 1.6.44, Expat 2.6.3, libxslt 1.1.42, libwebp 1.4.0, Brotli 1.1.0, WOFF2 1.0.2, curl 8.10.1, Little CMS 2.16, OpenJPEG 2.5.2 and Fontconfig 2.15.0.

### SQLite adapters

- The 50 `sqlite3_*` exports in [Sqlite.cpp](../../../core/libs/prx/libSceNKWebKitRequirements/src/Sqlite.cpp) assume the public SQLite C prototypes because their names hash to the inventory NIDs; the console prototypes are not confirmed. They call SQLite 3.54.1 built from `3rdparty/sqlite` with `SQLITE_THREADSAFE=1` and `SQLITE_OMIT_LOAD_EXTENSION`. The console library's version and compile options are unknown.
- [`sqlite3_open_v2`](../../../core/libs/prx/libSceNKWebKitRequirements/src/Sqlite.cpp) ignores the guest VFS argument. A `file:` URI passed directly to the export with `SQLITE_OPEN_URI` throws unless the path is exactly `:memory:` or the final `mode` is `memory`, and there are no percent escapes. For URIs evaluated by SQL, SQLite exposes the wrapped host default VFS and its memory-only `memdb` VFS. Other native VFS names are unavailable. Databases that exist only inside `app0.zar` cannot be opened. SQLite uses host sync calls rather than the guest file layer. Read-write opens record the database and its `-wal`, `-shm` and `-journal` paths for `sceKernelSync`.
- [`sqlite3_wal_hook`](../../../core/libs/prx/libSceNKWebKitRequirements/src/Sqlite.cpp) returns the host's previous argument, such as the default autocheckpoint frame count, until a guest hook is installed.

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceNKWebKitRequirements/Export.cpp) - 32 entries present in both version-labelled images are import-resolution declarations only. Neither label independently authenticates retail firmware provenance. These NIDs are absent from the pinned public 3.20 catalogue. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `++QInfMOBN0` |
| `Unknown` | `+oA7tlz2Aiw` |
| `Unknown` | `1MRhOnElUw4` |
| `Unknown` | `1T7K-PY5paE` |
| `Unknown` | `3yG30EKj-64` |
| `Unknown` | `4dTqu8uWd6Q` |
| `Unknown` | `6-rd4IXD0mM` |
| `Unknown` | `68aoiQnJutE` |
| `Unknown` | `8u1QXzSpyno` |
| `Unknown` | `JwB900c8Nyo` |
| `Unknown` | `KCP1Db-mniM` |
| `Unknown` | `NCGIxzXT-KA` |
| `Unknown` | `TAGTeghnmJs` |
| `Unknown` | `TNO-EUJZf3c` |
| `Unknown` | `Wz6vEl9ATDQ` |
| `Unknown` | `YpZ1fAOlVoo` |
| `Unknown` | `ZSkWe-fNsuY` |
| `Unknown` | `cgNin5rMqA8` |
| `Unknown` | `cq16C8ickjY` |
| `Unknown` | `hjlKZNuv+v8` |
| `Unknown` | `o6hmqtmcw10` |
| `Unknown` | `o7ktMsqiCBU` |
| `Unknown` | `rW6yS0aJbjQ` |
| `Unknown` | `rgWcud6b43M` |
| `Unknown` | `soW4BQ5l3Os` |
| `Unknown` | `tc5h+wVQWZI` |
| `Unknown` | `tu7+XEdsUA8` |
| `Unknown` | `u4SbDPju5eo` |
| `Unknown` | `uXf4oddEz5E` |
| `Unknown` | `uiTgPO1VRaA` |
| `Unknown` | `x2xMh95HEb4` |
| `Unknown` | `z5L7zirdMWM` |

### Functional

- [zlib exports](../../../core/libs/prx/libSceNKWebKitRequirements/Zlib.cpp) - console ABI, version and exact compressed bytes remain unverified. The named NIDs and upstream prototypes imply an LP64 112-byte stream; Linux and Windows use an explicit host-stream bridge. The backend is pinned upstream zlib 1.3.2, including gzip and automatic format detection. Bounds and compressed bytes can differ from older console versions. Host bookkeeping does not use guest `zalloc`; Windows clears a guest-set upper half of `adler` when synchronizing the host checksum. `uncompress` with a null length throws.

- [libjpeg exports](../../../core/libs/prx/libSceNKWebKitRequirements/Jpeg.cpp) have no console verification. The 15 decompression exports run libjpeg-turbo from `3rdparty/libjpeg-turbo`; the console's libjpeg version is unknown. Guest `jpeg_decompress_struct`, `jpeg_error_mgr`, `jpeg_source_mgr` and saved markers are LP64 mirrors copied to and from the host structures around each call, the input position also around each guest source callback; guest callbacks run through `APS5_VABI` trampolines. Errors raised inside libjpeg jump back to the export with `__builtin_longjmp`, so no C++ exception crosses libjpeg's frames; the adapter then calls the guest `error_exit`, and one that returns ends the export early. The default `error_exit` that `jpeg_std_error` installs throws `std::runtime_error` where libjpeg would call `exit`. A null object, or one not created by `jpeg_CreateDecompress`, throws `std::invalid_argument`. Closes when: compared with the console.

- [libxml2 exports](../../../core/libs/prx/libSceNKWebKitRequirements/Xml.cpp) have no console verification. The 23 exports run libxml2 2.9.14 from `3rdparty/libxml2`, built without iconv, ICU, HTTP, FTP or catalogs; the console's version and options are unknown. Guest-visible parser contexts, SAX handlers, documents, entities and errors are LP64 mirrors synced with the host objects around each call; callbacks run through `APS5_VABI` trampolines. An entity keeps one mirror until `xmlFreeDoc`, or for the process if predefined. `xmlFreeDoc` also clears `myDoc` in every context still pointing at the document, where libxml2 leaves it dangling. `xmlCtxtGetLastError` and the `xmlSAX2*` exports throw `std::invalid_argument` for a context not created by this library. Each successful `xmlRegisterInputCallbacks` or `xmlRegisterOutputCallbacks` keeps one of 15 slots for the life of the process; no export removes a registration. Closes when: compared with the console.

- [libpsl exports](../../../core/libs/prx/libSceNKWebKitRequirements/Psl.cpp) have no console verification. The 22 exports run libpsl 0.21.5 from `3rdparty/libpsl`; its built-in table is generated at build time from the pinned `list/public_suffix_list.dat`, so `psl_builtin` answers follow that snapshot, not the console's. File times and checksums returned by `psl_builtin_file_time` and `psl_builtin_sha1sum` describe that snapshot. Closes when: compared with the console.
