# libSceTtsCoreG3Bc technical debt

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceTtsCoreG3Bc/Export.cpp) - 14 entries present in both version-labelled images are import-resolution declarations only. A public 3.20 catalogue for this library was not verified. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `1scTM6aHK5I` |
| `Unknown` | `9tnCCarXqdY` |
| `Unknown` | `EmF4qOwLp5w` |
| `Unknown` | `H0VOaNk61es` |
| `Unknown` | `LQ65KkmJ48g` |
| `Unknown` | `PRmv-UzXa1w` |
| `Unknown` | `RByzVVv6e0A` |
| `Unknown` | `Sq5GSDo-Rhk` |
| `Unknown` | `bapD6H6e7Yk` |
| `Unknown` | `fF3Q3LdPUtk` |
| `Unknown` | `iW0DoaHZE5Y` |
| `Unknown` | `pacG6ffHAvU` |
| `Unknown` | `pzhkpUfnlqY` |
| `Unknown` | `stLW2n-xJ6M` |

- [`lQLzsMt-6NU`, `V+nR+xHS2ag`, `k1-orvzQrHw` and `+b5-z8Ff4cw`](../../../core/libs/prx/libSceTtsCoreG3Bc/Export.cpp) (libSceTtsCoreG3Bc) - their names and signatures are unverified; they keep the no-parameter declarations, and the ones whose firmware body is a lone `ret` return `void`. In firmware 13.42 `libSceTtsCoreG3Bc.sprx` each body returns at once without reading an argument: `lQLzsMt-6NU` and `V+nR+xHS2ag` return nothing; `k1-orvzQrHw` return 8; `+b5-z8Ff4cw` return 100. Read from disassembly, not measured on a console.

