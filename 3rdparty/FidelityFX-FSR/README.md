# FidelityFX-FSR

The shader headers of AMD FidelityFX Super Resolution 1.0. The AGC driver compiles them into the presentation's EASU and RCAS shaders, which run under `ANYPS5_UPSCALE=fsr`.

- Source: https://github.com/GPUOpen-Effects/FidelityFX-FSR
- Tag: v1.0.2, commit a21ffb8f6c13233ba336352bdff293894c706575
- License: MIT, in [license.txt](license.txt)

The files are copied unmodified from that commit, at the same paths:

| File | SHA-256 |
|------|---------|
| `ffx-fsr/ffx_a.h` | `f60e2722fcd13989523b9164d776ab382b3692791767f3bf8bb19967f763f3fb` |
| `ffx-fsr/ffx_fsr1.h` | `93c3922362ea7fc99cbcc698ca30c98de4f8c246d1fbb0b09e015ddef38ce3a5` |
| `license.txt` | `db089274ce766da70f5b7d791029c3486f9f9e27c8c79c652689603d3192e802` |

They are copied instead of added as a submodule because every release tag of that repository carries two submodules for AMD's sample, Cauldron and Cauldron-Media, which a recursive checkout would fetch: about 337 MB at depth 1 and 1.47 GB with full history, for 243 KB of headers.
