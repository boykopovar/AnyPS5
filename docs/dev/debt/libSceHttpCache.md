# libSceHttpCache technical debt

### Unknown function info

- [libSceHttpCache additional exports](../../../core/libs/prx/libSceHttpCache/Export.cpp) - these name/NID pairs are present in the inspected export inventory. Parameter lists, return types and behavior are unverified. Parameterless `int` declarations are import-resolution placeholders, not verified ABI prototypes; every entry throws through `NotImplemented_nid_no_patch`. No runtime functionality is implemented.

The inventory label is not authenticated retail-firmware provenance. The inspected image contains raw module-parameter words `0x13590001` at +0x10 and `0x13600007` at +0x14. Using the [published static ELF membership procedure](https://github.com/boykopovar/AnyPS5/pull/12367#issuecomment-6470603298), its 34 unique function NIDs have sorted-LF SHA-256 `7a05b0dd86d73e5eade84050580639fb242533a7f114ef96ffde7d259bde84e0`; all 2 entries below are present. This digest identifies the function-NID set, not the whole binary or its ABI. An independent review on [this PR](https://github.com/boykopovar/AnyPS5/pull/11934#pullrequestreview) corroborated these identities using a separately reported 13.42 reference; that does not establish matching binaries, prototypes or behavior.

| Name | NID |
| --- | --- |
| `sceHttpCacheEnableCacheInfo` | `dIQhFL4sKBo` |
| `sceHttpCacheUnregisterCacheInfoUrl` | `3JQc1T-fvmc` |

- [sceHttpCacheDebugSetUrl, sceHttpCacheSystemSendStatistics and sceHttpCacheSystemTerm](../../../core/libs/prx/libSceHttpCache/Export.cpp): the supplied implementations return zero without reading arguments. No callers have yet established whether their public prototypes include unused arguments.
- [`sceHttpCacheClearCacheInfo` and `sceHttpCacheRemoveTag`](../../../core/libs/prx/libSceHttpCache/Export.cpp): signatures unverified; parameterless declarations that throw through `NotImplemented_nid_no_patch`. The pointer types of [`sceHttpCacheClearAllUserTags`](../../../core/libs/prx/libSceHttpCache/Export.cpp) (a list of at most 20 entries, taken as user ids) and [`sceHttpCacheRegisterCacheInfoUrl`](../../../core/libs/prx/libSceHttpCache/Export.cpp) are inferred from the checks.

### Functional

- [libSceHttpCache](../../../core/libs/prx/libSceHttpCache/Export.cpp) - cached responses live in process memory, keyed by user, URL and `User-Agent` value, instead of the console's per-user files, so they are lost when the process exits; they survive `Term`. Request states, freshness, revalidation and header rules follow firmware 13.60, read from disassembly, not measured on a console. A `public` response stored while cache sharing is on, which the console files under the store shared by all users, throws through `NotImplemented_nid_no_patch`, as do a lookup and a `ClearAll` by another user while such an entry exists, since whether they reach the shared store was not read. The console's debug request header that adds cache state to responses is not reproduced. Header numbers the firmware's `stoul` or `stol` rejects, a null URL in `Retrieve` (the firmware dereferences it) and an empty header list to return throw.
- [libSceHttpCache](../../../core/libs/prx/libSceHttpCache/Export.cpp) - `SetQuota`, `ClearAllUserTags` and a non-null `RegisterCacheInfoUrl` throw through `NotImplemented_nid_no_patch` once their arguments pass; `Clear`, `CreateRequestWithTag`, `RemoveTag`, `ClearCacheInfo`, both `WithMemoryPool` forms, `SystemInit`, `SystemClearAll` and `SystemShutdown` throw outright, so no entry has a tag or a quota. `Init` ignores its pool size, so the pool allocation failure is not reproduced, and storing never fails, so the console's file errors are not reproduced.
- [libSceHttpCache](../../../core/libs/prx/libSceHttpCache/Export.cpp) - the freshness lifetime is `Expires` minus `Date`, or a tenth of `Date` minus `Last-Modified`, computed on unsigned ticks without comparing them first, and an entry is fresh while its age is below that lifetime, as in firmware 13.60, read from disassembly, not measured on a console. An `Expires` earlier than `Date`, or a `Last-Modified` later than `Date`, therefore wraps to a lifetime that never runs out, so the entry keeps being returned as a hit instead of going stale as RFC 9111 asks; this is kept to match the console.

- [Unidentified export `Y3bCbdeHzNg`](../../../core/libs/prx/libSceHttpCache/Export.cpp) appears in the 3.20 and later version-labelled inventories. Its name, parameters, return type and behavior are unverified. The no-parameter import-resolution declaration throws through `NotImplemented_nid_no_patch`.
