# libSceAudioPropagation technical debt

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceAudioPropagation/InventoryExports.cpp) - 8 entries present in both version-labelled images are import-resolution declarations only. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `sceAudioPropagationSourceQueryAudioPaths` | `yW8FldaufkU` |
| `Unknown` | `TrQMW7ARZXY` |
| `Unknown` | `ZSdiF+pXrF8` |
| `Unknown` | `fOTrQvqzABM` |
| `Unknown` | `fhMkXxnE8aw` |
| `Unknown` | `laU0fM-tTsc` |
| `Unknown` | `mdH4mANi9GI` |
| `Unknown` | `o0gGIp1vd6A` |

### Silent stubs

- [libSceAudioPropagation](../../../core/libs/prx/libSceAudioPropagation/Export.cpp) lacks acoustic propagation: attributes have no effect, sources expose 36 initially empty, invalid path slots and no rays, rendering preserves the destination mix without adding path contributions, and allocated CPU work memory is mostly unused. Ray results and audio paths accept empty lists and throw otherwise; `sceAudioPropagationSourceCalculateAudioPaths` returns the initial native validation errors, then goes through `NotImplemented_nid_no_patch`, as propagation is not implemented. Room, portal, source and material handles are tracked, but destruction after their system is destroyed silently succeeds without native lifetime semantics being confirmed. Occlusion, reflections and reverb are absent.

### Unknown function info

- [sceAudioPropagationSourceSetAudioPath](../../../core/libs/prx/libSceAudioPropagation/Export.cpp): native evidence establishes a 64-bit path handle, a read-only 224-byte path description and a float gain. The four 48-byte points contain a room handle, gain and position; trailing eight-byte fields remain opaque. Updates copy the description and validate room ownership. Each source owns 36 initially invalid, empty slots, following public SharpProspero MaxPaths and SAROS's immediate lookup of slot 0. Allocation before calculation, the count of 36 and update validation precedence are inferred, not measured on a console. Native lookup error codes and their handle/null/index precedence are retained. Source/system destruction invalidates owned slots. Active nonempty path rendering still throws.
- [libSceAudioPropagation](../../../core/libs/prx/libSceAudioPropagation/Export.cpp): system options use a recovered 56-byte record; the field at byte 24 and final eight bytes remain semantically unresolved. CPU memory requirements and option limits are recovered; GPU memory queries throw because their platform-specific runtime condition is unresolved. Descriptor validation, ray ownership and path generation are unimplemented. Native rendering accepts multiple mutable records; the host accepts public formats 0/1/2/4/5 (4/9/16/8/8 float channels), preserves the output mix without path contributions and clears the returned channel count; non-format-2 behavior remains unverified on a console. Frame capacity and source-count limits follow the recovered options. Other formats throw; native validation precedence remains unimplemented.
- [Initialization exports](../../../core/libs/prx/libSceAudioPropagation/Export.cpp) ending in `Init`, plus `sceAudioPropagationSystem`, are not exported: the SDK declares the helpers `static inline` and none is in the SDK stub library or the supplied native module. Query, locking, enumeration and report callback signatures are recovered; diagnostic layouts are recovered, but source attributes 2/3 and the constant-one fields still lack semantic names. The final system word and the fourth position/direction words have no confirmed semantics.
