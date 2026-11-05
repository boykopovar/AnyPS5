# Party technical debt

### Unknown function info

- [Party](../../../core/libs/prx/Party/Export.cpp) (Party) - 26 functions that titles import from the `Party` module, taken from the imports of PPSA02632 (01.000.000). Their names, parameters and behaviour are unknown: no `sceParty` or `sceNpParty` name tried hashes to any of them. Each is a literal NID alias that throws through `NotImplemented_nid_no_patch`, and the parameterless `int` declarations are placeholders, not verified signatures. 16 of them are also exported by libkernel as `sceKernelUnknown03` to `sceKernelUnknown19` (without `sceKernelUnknown16`); a Windows import is bound to the library it names, so those copies do not resolve imports from `Party`

| Name | NID |
| --- | --- |
| `Unknown` | `0iEKNAvT600` |
| `Unknown` | `2cELEBPdYQ0` |
| `Unknown` | `5xYxiOtOccA` |
| `Unknown` | `6W+shIH315Q` |
| `Unknown` | `7ilObq815O4` |
| `Unknown` | `7iq7Hfs8HBk` |
| `Unknown` | `7kwSdZNDyP0` |
| `Unknown` | `AC1FtjqMCL0` |
| `Unknown` | `AJ3I035AjIM` |
| `Unknown` | `AMJZhpC+siY` |
| `Unknown` | `aq13EdrGdVw` |
| `Unknown` | `BOW-t-9qJNI` |
| `Unknown` | `E9XkHowt+dY` |
| `Unknown` | `f3d6tchKw94` |
| `Unknown` | `GADjreszOgM` |
| `Unknown` | `Giyg2xGrXDU` |
| `Unknown` | `JrVwjbBmqLQ` |
| `Unknown` | `KC3y21wrqzc` |
| `Unknown` | `nVzRzo3Yr+s` |
| `Unknown` | `pnz21I6VjDs` |
| `Unknown` | `Qb8wS4GZUsw` |
| `Unknown` | `qk043QPsrZU` |
| `Unknown` | `QVQ-lvlaJZw` |
| `Unknown` | `SjQdzD78RpM` |
| `Unknown` | `uDGXwUKqi8s` |
| `Unknown` | `YdBvG5Myxzg` |
