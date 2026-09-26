# Tools

## progress.py

Generates the progress map shown in the main [README](../README.md).

```sh
python3 tools/progress.py <output-dir>
```

Requires only Python 3.9+ with the standard library. It writes:

- `progress.svg`: treemaps for system libraries and GPU shader instructions
- `badge-libraries.svg`, `badge-shaders.svg`: percentage badges
- `README.md`: per-library and per-encoding tables
- `progress.json`: raw numbers

The [Progress workflow](../.github/workflows/progress.yml) runs it on every push to `main` and publishes the output to the `progress` branch.

### What is measured

- **System libraries**: functions defined in [core/libs/prx](../core/libs/prx). A function is implemented when it no longer calls `NotImplemented_nid_no_patch`. The total only includes functions already declared in the project, not every function of the PS5 firmware.
- **GPU shader instructions**: instructions from [rdna_isa.txt](rdna_isa.txt) that appear in `RdnaOpcode` of the [shader recompiler](../core/shader/recompiler/RdnaDecoder). FLAT opcodes also count for their GLOBAL and SCRATCH forms, since the decoder accepts all three segments. Decoded opcodes missing from AMD's public list are not counted.

## rdna_isa.txt

The AMD RDNA 1 + RDNA 2 instruction list (name and primary encoding), extracted from the [AMD Machine-Readable GPU ISA Specification](https://gpuopen.com/machine-readable-isa/) (MIT License).
