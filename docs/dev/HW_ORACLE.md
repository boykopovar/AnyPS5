# Hardware oracle

`tools/hw-oracle/` runs a few lines of RDNA assembly on a local AMD GPU, one input row per lane, and returns what the
hardware computed. Use it to settle instruction semantics that the ISA documentation, LLVM or Mesa leave open, then pin the
measured rows as expected values in `core/libs/prx/libSceAgcDriver/tests/execution/`.

The PS5 GPU is gfx1013. Other RDNA2 GPUs (gfx103x) share the VALU, SALU, DS, MUBUF and MIMG encodings, apart from ops that
exist only on gfx10.1 (`v_mad_legacy_f32`, `v_mac_legacy_f32`, `v_mul_lo_i32`, `s_dcache_discard`, ...). RDNA3 also runs
the tool, but differs from gfx1013 in more places ([RDNA3 differences](#rdna3-gfx11-differences)). Say which GPU you
measured on in the PR.

## Requirements

### Linux

- Linux with the `amdgpu` driver and read/write access to `/dev/kfd` (usually the `render` group). Root is not needed.
- The ROCm HSA runtime with its headers: `libhsa-runtime-dev` (Debian, Ubuntu), `rocm-runtime-devel` (Fedora),
  `hsa-rocr` (Arch), or AMD's ROCm packages. A runtime under `ROCM_PATH` or `/opt/rocm` is used first. The rest of ROCm
  is not needed.
- A C compiler, and `clang` + `ld.lld` with the AMDGPU target (`llc --version` lists `amdgcn`). If they aren't in
  `PATH`, they are taken from `$ROCM_PATH/llvm/bin`.

The first run builds `oracle.c` into `$XDG_CACHE_HOME/anyps5-hw-oracle/`, or `~/.cache/anyps5-hw-oracle/` when
`XDG_CACHE_HOME` is unset, empty or relative (`HW_ORACLE_CACHE` overrides both). The target is read from the GPU (`HW_ORACLE_TARGET`
overrides it).

### Windows

- AMD's Windows driver, which installs the HIP runtime (`amdhip64_7.dll`, `amdhip64_6.dll` or `amdhip64.dll`) and the code object manager (`amd_comgr_3.dll`, `amd_comgr_2.dll` or `amd_comgr.dll`)
  in `System32`. ROCm, a C compiler, `clang` and `ld.lld` are not needed.
- Python 3 from python.org or the Microsoft Store.

On Windows `oracle_hip.py` replaces `oracle.c`: comgr assembles and links the kernel, and HIP runs it. The CLI, the Python
interface and the results are the same as on Linux. `HW_ORACLE_RUNTIME` picks the runtime (`hip` on Windows, `hsa`
elsewhere; `vulkan` is never the default, see [Recompiler comparison](#recompiler-comparison)). `HW_ORACLE_HIP` and `HW_ORACLE_COMGR` name other DLLs. `ROCR_VISIBLE_DEVICES` selects the GPU, through
`HIP_VISIBLE_DEVICES`, unless that variable is already set.

## Kernel

`template.s` wraps the body under test. Per lane:

- `v4`-`v7` hold the row's 4 input dwords. `v10`-`v25` start at 0 and are stored as the result (16 dwords).
- Leave `v0` (lane id), `v1` (input offset), `v2` (output offset) and `s[4:7]` (input and output addresses) unchanged.
- 4 KiB of LDS, or the group segment size `--lds` (Python `lds`) gives in bytes, up to 64 KiB. Rows run in dispatches of up to
  1024 rows (512 with wave64), one workgroup each, padded to whole waves with zero rows.
- Bytes passed as `extra` start at `s[4:5] + 16 * extra_rows`, where `extra_rows` is the padded row count for one
  dispatch and the dispatch capacity for multiple dispatches (1024 in wave32, 512 in wave64). Tail dispatches reserve
  unused row slots so the address stays fixed. Build buffer and image descriptors in SGPRs from that address. A kernel
  that needs more than 4 input dwords per lane takes the rest from there too, for example `v_lshlrev_b32 v40, 3, v0` /
  `v_add_nc_u32 v40, 16 * extra_rows, v40` / `global_load_dwordx2 v[8:9], v40, s[4:5]`.

Every run must explicitly set all floating-point controls, including runs of integer instructions. The CLI flags
use hyphens; Python keywords use underscores. Record these settings alongside the GPU and measured results.

| Python keyword | Values |
| --- | --- |
| `ieee` | 0 or 1: IEEE mode disabled or enabled |
| `dx10_clamp` | 0 or 1: DX10 clamp disabled or enabled |
| `denorm32`, `denorm16` | 0: flush input/output; 1: preserve input, flush output; 2: flush input, preserve output; 3: preserve both |
| `round32`, `round16` | 0: nearest even; 1: toward +infinity; 2: toward -infinity; 3: toward zero |
| `fp16_overflow` | 0: overflow to infinity; 1: clamp computed overflow to the largest finite value (an infinite source operand still produces infinity, as does `v_div_fixup_f16` for a zero denominator or an infinite numerator, but not for an infinite quotient; `v_rcp_f16`, `v_rsq_f16` and `v_log_f16` of zero are clamped) |

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

## Curated probe catalog

[`probe_catalog.py`](../../tools/hw-oracle/probe_catalog.py) provides a small checked-in set of explicit, reviewable
instruction probes. The catalog starts with f32 and f16 arithmetic edge cases, including signed zero, NaNs, rounding
ties, denormals and overflow modes. It does not generate instruction semantics from the ISA form list and it stores no
expected outputs. A measurement is only the result returned by the configured hardware oracle.

List the curated cases, then generate portable input bundles without a GPU:

```sh
python3 tools/hw-oracle/probe_catalog.py list
python3 tools/hw-oracle/probe_catalog.py generate --output-dir /tmp/anyps5-probes \
  --probe v-add-f32-edge-values
```

Each generated case directory contains `body.s`, `rows.txt` and `probe.json`. These files can also be passed to
`hw_oracle.py` directly, with the required floating-point options from `probe.json`.

To run the catalog, provide a user-chosen label for the machine and GPU, then ask for the report summary:

```sh
python3 tools/hw-oracle/probe_catalog.py run --results-dir measurements --device-label rx-6750-xt
python3 tools/hw-oracle/probe_catalog.py coverage --results-dir measurements --device-label rx-6750-xt
```

`run` measures every catalog entry by default; repeat `--probe ID` to select a subset. It does not overwrite an existing
measurement file. Reports include the hardware-oracle runtime and assembly target, the exact catalog digest, all input
and output words, and the user-supplied device label. The label is not checked against the physical GPU, and the
assembly target alone does not identify a GPU model or driver. `coverage` checks the individual probe definition
digest, inputs and settings. Adding another probe leaves existing measurements valid; editing a probe marks only that
probe's old report stale. It reports missing, stale or invalid files separately and exits 1 when the selected probe set
is incomplete. Use `--json` for machine-readable coverage data.

## Recompiler comparison

The `vulkan` runtime runs the same assembled kernel through the shader recompiler on the Vulkan device instead of on AMD
hardware, so one body and one set of rows can be measured on both and the results compared. Build the runner:

```sh
cmake --build build --target agc_oracle_runner
```

| Variable | Meaning |
| --- | --- |
| `HW_ORACLE_RUNTIME=vulkan` | selects the runtime |
| `HW_ORACLE_VULKAN_RUNNER` | path of the built `agc_oracle_runner` executable |
| `ANYPS5_GPU` | case-insensitive substring of the Vulkan device name, as for every AnyPS5 tool |

On Windows the runner needs `build/core/libs/libs/unpatched` in `PATH` ([BUILD](BUILD.md)). Run the hardware with the default
runtime and the recompiler with `vulkan`, with the same body, rows and settings, each with `--json`, then compare:

```sh
SETTINGS="--ieee 0 --dx10-clamp 1 --denorm32 0 --denorm16 3 --round32 0 --round16 0 --fp16-overflow 0"
python3 tools/hw-oracle/hw_oracle.py body.s rows.txt --json $SETTINGS > hardware.json
HW_ORACLE_RUNTIME=vulkan HW_ORACLE_VULKAN_RUNNER=<path to agc_oracle_runner> \
  python3 tools/hw-oracle/hw_oracle.py body.s rows.txt --json $SETTINGS > recompiler.json
python3 tools/hw-oracle/compare_results.py hardware.json recompiler.json
```

The report's `runtime` field says which side produced it.

- Compute only.
- Whole waves: Python pads the rows with zeros to the wave size, so cross-lane instructions see the padded lanes.
- At most 1024 lanes per dispatch (512 with `--wave64`).
- No `coarse`.
- The assembly target comes from the AMD driver on Windows or from `HW_ORACLE_TARGET`; the recompiler has no target of its
  own, and the encoding assembled is the RDNA2 one.

## RDNA3 (gfx11) differences

Measured on gfx1151 (Radeon 8060S, AMD Windows driver 32.0.22032.6002, HIP runtime). The PS5's gfx1013 is not measured
here; a result that depends on these differences needs a run on RDNA2.

| gfx10 | gfx11 |
| --- | --- |
| `ds_cmpst_b32/f32/b64/f64` and their `_rtn` forms | Not accepted. `ds_cmpstore_*` takes the value to store as `data0` and the comparand as `data1`, the reverse of `ds_cmpst_*`, so renaming alone stores the comparand when memory equals the new value. |
| `v_fmac_f32_e64`, `v_fmac_f16_e64` with a third source | The assembler takes two sources only, so `src2` modifiers can't be written. Emit the VOP3 words with `.long` (opcode `0x12b`, `0x136`, `src2` = `vdst`); `neg` bit 2 then negates the accumulator as in `v_fma_*`. |
| `v_mad_f32`, `v_mac_f32`, `v_madak_f32`, `v_madmk_f32`, `v_mad_f16`, `v_mac_f16`, `v_mad_legacy_f32`, `v_mac_legacy_f32`, `v_mul_lo_i32`, `s_dcache_discard` | Not accepted. |
| SDWA (`_sdwa`) | Not accepted. |
| `v_interp_p1_f32`, `v_interp_mov_f32` | Not accepted, so the LDS interpolation under [Limits](#limits) is RDNA2 only. |

`v_mul_legacy_f32`, `ds_wrxchg2_rtn_*` and `ds_wrxchg2st64_rtn_*` still assemble under these names and give the same
results as their gfx11 names `v_mul_dx9_zero_f32`, `ds_storexchg_2addr_rtn_*` and `ds_storexchg_2addr_stride64_rtn_*`.

## Limits

Only compute kernels. Pixel-shader ops that read their inputs from LDS also work: `v_interp_*` reads the attribute
parameters from LDS at `M0`, so writing them there by hand is enough. Behaviour that depends on the graphics pipeline
(parameter cache, exports, rasterization, `SPI_*` state) can't be measured with it.

Argument handling can be tested without a GPU:

```sh
python3 tools/hw-oracle/test_hw_oracle.py
```
