# libSceNKWebKitRequirements technical debt

### Known export names; ABI prototypes unverified

- 83 pending exports in [Export.cpp](../../../core/libs/prx/libSceNKWebKitRequirements/Export.cpp) are present in the [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs) inventory. Their prototypes and behavior are unverified. They use the no-parameter import-resolution convention and throw through `NotImplemented_nid_no_patch`; these declarations make no ABI or behavior claim.
- The 72 of them declared in [Identified.hpp](../../../core/libs/prx/libSceNKWebKitRequirements/include/Identified.hpp) carry the zlib, libjpeg, libpsl, libxml2 and UndefinedBehaviorSanitizer names whose `tools/nid_names.py` hash equals the inventory NID; the names were taken from the identifiers of the zlib 1.3.1, libjpeg-turbo 3.0.4, libpsl 0.21.5, SQLite 3.46.1 and libxml2 2.13.4 sources and from the two `__ubsan_handle_cfi_check_fail` handlers libkernel_sys exports. A name match does not show that the console function has that library's prototype or behavior. The 50 `sqlite3_*` names of the same inventory are implemented, see below. The other 11 hash to no identifier of those sources or of ICU 76.1, HarfBuzz 10.0.1, cairo 1.18.2, pixman 0.42.2, FreeType 2.13.3, libpng 1.6.44, Expat 2.6.3, libxslt 1.1.42, libwebp 1.4.0, Brotli 1.1.0, WOFF2 1.0.2, curl 8.10.1, Little CMS 2.16, OpenJPEG 2.5.2 and Fontconfig 2.15.0.

### SQLite exports

- The 50 `sqlite3_*` exports in [Sqlite.cpp](../../../core/libs/prx/libSceNKWebKitRequirements/Sqlite.cpp) run on SQLite 3.46.1, compiled from the amalgamation in the [3rdparty/sqlite](../../../3rdparty/sqlite) submodule by [Sqlite.cmake](../../../cmake/Sqlite.cmake). The submodule is the third-party mirror [litalidev/sqlite.amalgamation](https://github.com/litalidev/sqlite.amalgamation) at tag `version-3.46.1`, not sqlite.org: its `sqlite3.h` reports `SQLITE_VERSION` 3.46.1 and a `SQLITE_SOURCE_ID` whose hash equals the `manifest.uuid` of the official `version-3.46.1` tag, but the files were not diffed against the sqlite.org archive. 3.46.1 is the version whose identifiers matched the inventory NIDs; no source establishes the SQLite version the console ships, so the pin is a choice and not a finding. `sqlite3_libversion` is not among the exports, so no export reads the version. Messages from `sqlite3_errmsg` and `sqlite3_errstr` follow 3.46.1 and may differ from the console build.
- The prototypes in [Identified.hpp](../../../core/libs/prx/libSceNKWebKitRequirements/include/Identified.hpp) are the public SQLite 3.46.1 ones. The inventory names do not prove them, and no console trace or binary was used to confirm the calling convention, the `int` widths or the callback signatures.
- The build options of [CMakeLists.txt](../../../core/libs/prx/libSceNKWebKitRequirements/CMakeLists.txt) are the defaults plus `SQLITE_THREADSAFE=1` and `SQLITE_OMIT_LOAD_EXTENSION`. The options the console build uses are unknown: FTS3, FTS5, R-tree, session, RBU, ICU and `SQLITE_DEBUG` behaviour, the default page size and the mutex mode are not established.
- `sqlite3_open_v2` in [Sqlite.cpp](../../../core/libs/prx/libSceNKWebKitRequirements/Sqlite.cpp) accepts a path, `:memory:` and the empty temporary-database name. The path goes through the guest filesystem resolver and the host receives the resolved UTF-8 path. A path the resolver refuses returns `SQLITE_CANTOPEN` with a null handle, where SQLite would return a handle that carries the error. A named VFS and `file:` URIs under `SQLITE_OPEN_URI` throw: the console VFS names and the URI-to-guest-path mapping are unknown.
- A `sqlite3_bind_blob` or `sqlite3_bind_text16` destructor in [Sqlite.cpp](../../../core/libs/prx/libSceNKWebKitRequirements/Sqlite.cpp) that is neither `SQLITE_STATIC` nor `SQLITE_TRANSIENT` is called inside the bind call after the data is copied, not when SQLite releases the value. The call also happens when the bind fails, as in SQLite, and does not happen for a null data pointer.
- In [Sqlite.cpp](../../../core/libs/prx/libSceNKWebKitRequirements/Sqlite.cpp), the busy handler, authorizer and WAL hook callbacks are kept per connection and released when `sqlite3_close` succeeds, when the hook is replaced or removed, and, for the busy handler, when `sqlite3_busy_timeout` replaces it. Replacing a hook while another thread runs it is not synchronised with that call.
- In [Sqlite.cpp](../../../core/libs/prx/libSceNKWebKitRequirements/Sqlite.cpp), `sqlite3_create_function` callbacks reach `sqlite3_user_data` and `sqlite3_result_error` through the context SQLite passes to them. `sqlite3_result_error` and `sqlite3_user_data` are the only context accessors among the exports, so a registered function can raise an error but cannot return a value until the `sqlite3_result_*` and `sqlite3_aggregate_context` exports are added.

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
