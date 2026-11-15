# libSceFace technical debt

### Unknown function info

- [Additional libSceFace exports](../../../core/libs/prx/libSceFace/Export.cpp) - the 28 name/NID pairs below are defined function exports recovered from the GTAVC-bundled `libSceFace.prx` export table (the 4.03 firmware dump does not ship this module; the GTAVC bundle is the only concrete binary evidence). Their signatures and behavior are unknown; parameterless `int` declarations are import placeholders, not verified ABI prototypes. Every call throws through `NotImplemented_nid_no_patch`. No detection, recognition, pose, shape, or attribute analysis is implemented.

| Name | NID |
| --- | --- |
| `sceFaceAgeRangeEstimate` | `R0lsCgrINWo` |
| `sceFaceAgeRangeIntegrate` | `mrF0OjeuOgw` |
| `sceFaceAllParts` | `V1f-bRIanh4` |
| `sceFaceAllPartsGetWorkingMemorySize` | `Go4qB43MgX8` |
| `sceFaceAttribute` | `ibYWv3khTDw` |
| `sceFaceAttributeGetWorkingMemorySize` | `FTnplTtBnw8` |
| `sceFaceAgeGetWorkingMemorySize` | `EMiyK-GRjA8` |
| `sceFaceDetection` | `lHTcSjAnKdI` |
| `sceFaceDetectionEx` | `mz0+WE5XkSc` |
| `sceFaceDetectionGetWorkingMemorySize` | `DYdXC4Ixa4s` |
| `sceFaceDetectionLocal` | `j7lvkiMu1Sg` |
| `sceFaceEstimatePoseRegion` | `rfnPKQVKc4A` |
| `sceFaceIdentifyExGetFeature` | `AsxmyXFDsiM` |
| `sceFaceIdentifyExGetWorkingMemorySize` | `kZ5Cc0KqzdI` |
| `sceFaceIdentifyExSimilarity` | `+0E0dyw8phY` |
| `sceFaceIdentifyGetFeature` | `AfI6pPRlI34` |
| `sceFaceIdentifyGetWorkingMemorySize` | `jCSXa2n1cXI` |
| `sceFaceIdentifyLiteGetFeature` | `iQuWNdsul1U` |
| `sceFaceIdentifyLiteGetWorkingMemorySize` | `pk4p3CEJflE` |
| `sceFaceIdentifyLiteSimilarity` | `+sAja0b9rZU` |
| `sceFaceIdentifySimilarity` | `37DR-ZKbsv8` |
| `sceFaceParts` | `rZYoZcqqJdo` |
| `sceFacePartsEx` | `BBM2NN4R+Xs` |
| `sceFacePartsGetWorkingMemorySize` | `uiyG1M9D8-4` |
| `sceFacePartsResultCheck` | `WzOBQR6MNWA` |
| `sceFaceShapeFit` | `4eUBMHs41k0` |
| `sceFaceShapeGetWorkingMemorySize` | `BsmV0JLSdpg` |
| `sceFaceShapeTrack` | `47S+J03eask` |

### Implemented

- [`sceFaceDetectionGetDefaultParam`](../../../core/libs/prx/libSceFace/Export.cpp) `(void *param)` - verified against the GTAVC module body (entry `0x1f80`, size `0x42`): null output returns `0x808b0002`; otherwise a 0x38-byte defaults block is written verbatim: `{u32 56, f32 1.0, f32 0.841 (0x3f574bc7), u32 13200}` at `+0x00`, `0` at `+0x10`, `{u32 0, 0, 2, 2}` at `+0x14`, `{u32 0, 0, f32 0.5, u32 1}` at `+0x24`, `0` at `+0x34`. Field semantics are not published; the bytes are copied exactly as the binary writes them.

### Partially decoded, not yet ported

- [The nine `*GetWorkingMemorySize` exports](../../../core/libs/prx/libSceFace/Export.cpp) share one body shape: validate dimensions and the parameter pointer, canonicalize the caller parameter struct (internal deserializer helpers), run a per-family size calculator that recurses into per-record virtual methods, add a fixed overhead, and return `0` on invalid input. The deserializer pattern and the `FACE`/`PD33`, `PART`/`ALLP`/`PD33`, and `MSHP`/`v104` parameter-blob magics are documented in the harvest notes; the deepest calculators were not fully traced.
- [`sceFacePartsResultCheck`](../../../core/libs/prx/libSceFace/Export.cpp) validates a `PART` magic, check/version values `CHK1`/`CHK2`/`CHK3`, size consistency and a four-part result mask/quorum, then normalizes the result structures; the trailing normalization helpers are only partially traced.
- [`sceFaceEstimatePoseRegion`](../../../core/libs/prx/libSceFace/Export.cpp) is a quadratic pose-fit (~500 instructions plus nested helpers) and was not ported.
- [The detection/identify/shape/attribute/age exports](../../../core/libs/prx/libSceFace/Export.cpp) dispatch into internal analysis engines; no honest trivial implementation exists.
