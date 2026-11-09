# libCprm technical debt

### Callable exports

- [Callable exports](../../../core/libs/prx/libCprm/Export.cpp) - 35 entries are import-resolution declarations only. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `+Eh9ZNYi6zo` |
| `Unknown` | `+X9TC+1sw8c` |
| `Unknown` | `1M23A00Z5YY` |
| `Unknown` | `9Blq0Y1cfC4` |
| `Unknown` | `C+UDpkvBfq0` |
| `Unknown` | `Dxqbh2fid-0` |
| `Unknown` | `GYIfXUO2DQM` |
| `Unknown` | `GzdjmaMs2ec` |
| `Unknown` | `Ja2HmL89pQk` |
| `Unknown` | `N1QA8ydpT5s` |
| `Unknown` | `Qqt9wsrnORY` |
| `Unknown` | `SebG2x8vugA` |
| `Unknown` | `Sz5wfo9uoQo` |
| `Unknown` | `UXwPU6S3K3M` |
| `Unknown` | `Ut6wGp0l-EU` |
| `Unknown` | `Uv-1Y7ctRPg` |
| `Unknown` | `WWfjXT2d7SM` |
| `Unknown` | `YhPUdyQoZ2w` |
| `Unknown` | `YvBd1vant8Y` |
| `Unknown` | `aoE6klVaQmo` |
| `Unknown` | `fb1srL++42M` |
| `Unknown` | `gnClz0AOgtw` |
| `Unknown` | `h9jXWgBLon0` |
| `Unknown` | `hs4OD9CTzsU` |
| `Unknown` | `hvZH8f+HBVg` |
| `Unknown` | `i2aJI0V3U3A` |
| `Unknown` | `iDCilRZjGRM` |
| `Unknown` | `lCmvZpz-8Sg` |
| `Unknown` | `llRRCXKDB8g` |
| `Unknown` | `mq+QSu3eTDM` |
| `Unknown` | `pa1PYEalrtU` |
| `Unknown` | `tsJszAZ7k+A` |
| `Unknown` | `vR2Jy582NrI` |
| `Unknown` | `x5bhPjRC+WQ` |
| `Unknown` | `y2mH4WVl028` |

### Unresolved data exports

These older data exports remain undeclared. Their NIDs and sizes also appear in the later inventory, but their types, initialization and lifetime have not been established. Both images contain the same relocation pattern within these objects: the first has five relative relocations; the second has four relative relocations and one symbol relocation. The file-backed bytes are zero before relocation. Object type, relocated targets, initialization and lifetime remain unverified.

| NID | Size (bytes) | Relocations |
| --- | ---: | ---: |
| `5vIwXuuJRyw` | 56 | 5 |
| `xPsa6JbPY0w` | 56 | 5 |
