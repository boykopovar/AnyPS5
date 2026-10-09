# Hardware oracle

`tools/hw-oracle/` runs a few lines of RDNA assembly on a local AMD GPU, one input row per lane, and returns what the
hardware computed. Use it to settle instruction semantics that the ISA documentation, LLVM or Mesa leave open, then pin the
measured rows as expected values in `tests/execution/`.

The PS5 GPU is gfx1013. Other RDNA2 GPUs (gfx103x) share the VALU, SALU, DS, MUBUF and MIMG encodings, apart from ops that
exist only on gfx10.1 (`v_mad_legacy_f32`, `v_mac_legacy_f32`, `v_mul_lo_i32`, `s_dcache_discard`, ...). RDNA3 also runs
the tool, but differs from gfx1013 in more places. Say which GPU you measured on in the PR.

## Requirements

- Linux with the `amdgpu` driver and read/write access to `/dev/kfd` (usually the `render` group). Root is not needed.
- The ROCm HSA runtime with its headers: `libhsa-runtime-dev` (Debian, Ubuntu), `rocm-runtime-devel` (Fedora),
  `hsa-rocr` (Arch), or AMD's ROCm packages. A runtime under `ROCM_PATH` or `/opt/rocm` is used first. The rest of ROCm
  is not needed.
- A C compiler, and `clang` + `ld.lld` with the AMDGPU target (`llc --version` lists `amdgcn`). If they aren't in
  `PATH`, they are taken from `$ROCM_PATH/llvm/bin`.

The first run builds `oracle.c` into `~/.cache/anyps5-hw-oracle/` (`HW_ORACLE_CACHE` overrides it). The target is read
from the GPU (`HW_ORACLE_TARGET` overrides it).

## Kernel

`template.s` wraps the body under test. Per lane:

- `v4`-`v7` hold the row's 4 input dwords. `v10`-`v25` start at 0 and are stored as the result (16 dwords).
- Leave `v0` (lane id), `v1` (input offset), `v2` (output offset) and `s[4:7]` (input and output addresses) unchanged.
- 4 KiB of LDS, or the group segment size `--lds` (Python `lds`) gives in bytes, up to 64 KiB. Rows run in workgroups of up to
  1024 lanes, padded to whole waves with zero rows.
- Bytes passed as `extra` follow the rows of each dispatch, at `s[4:5] + 16 * rows`: buffer contents, texels, etc.
  Build buffer and image descriptors in SGPRs from that address. A kernel that needs more than 4 input dwords per lane
  takes the rest from there too, for example `v_lshlrev_b32 v40, 3, v0` / `v_add_nc_u32 v40, 16 * rows, v40` /
  `global_load_dwordx2 v[8:9], v40, s[4:5]` for two more dwords per lane, with `rows` the padded count.

Every run must explicitly set all floating-point controls, including runs of integer instructions. The CLI flags
use hyphens; Python keywords use underscores. Record these settings alongside the GPU and measured results.

| Python keyword | Values |
| --- | --- |
| `ieee` | 0 or 1: IEEE mode disabled or enabled |
| `dx10_clamp` | 0 or 1: DX10 clamp disabled or enabled |
| `denorm32`, `denorm16` | 0: flush input/output; 1: preserve input, flush output; 2: flush input, preserve output; 3: preserve both |
| `round32`, `round16` | 0: nearest even; 1: toward +infinity; 2: toward -infinity; 3: toward zero |
| `fp16_overflow` | 0: overflow to infinity; 1: clamp computed overflow to the largest finite value (infinite inputs and division by zero still produce infinity) |

`denorm16` and `round16` control both f16 and f64. These values follow the
[LLVM AMDGPU kernel descriptor documentation](https://llvm.org/docs/AMDGPUUsage.html#amdhsa-kernel-descriptor).
Float atomics need `coarse`: on fine-grained system memory they do nothing.

## Use

```sh
cat > body.s <<'EOF'
  v_add_f16 v10, v4, v5
EOF
printf '0x3c00 0x3c00 0 0\n0x7e01 0x3c00 0 0\n' > rows.txt
python3 tools/hw-oracle/hw_oracle.py body.s rows.txt --outs 1 \
  --ieee 0 --dx10-clamp 1 --denorm32 0 --denorm16 3 \
  --round32 0 --round16 0 --fp16-overflow 0
```

prints one line per row with the output dwords in hex. From Python:

```python
import sys
sys.path.insert(0, "tools/hw-oracle")
from hw_oracle import run

rows = [(a, b, 0, 0) for a in values for b in values]
results = run(body, rows, wave64=True, ieee=0, dx10_clamp=1,
              denorm32=0, denorm16=3, round32=0, round16=0, fp16_overflow=0)
```

Usual workflow: run the instruction on rows that separate the candidate models (edge values, NaN payloads, rounding
ties), write a model that matches every row, implement it, and put measured rows in the execution test.

## Limits

Only compute kernels. Pixel-shader ops that read their inputs from LDS also work: `v_interp_*` reads the attribute
parameters from LDS at `M0`, so writing them there by hand is enough. Behaviour that depends on the graphics pipeline
(parameter cache, exports, rasterization, `SPI_*` state) can't be measured with it.

Argument handling can be tested without a GPU:

```sh
python3 tools/hw-oracle/test_hw_oracle.py
```
