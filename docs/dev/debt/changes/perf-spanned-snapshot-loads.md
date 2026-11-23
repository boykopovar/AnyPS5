# shader

## add Functional

- [Spanned snapshot loads](../../../core/shader/recompiler/SpirvBackend/src/SpirvSpannedUniformity.cpp) (shader recompiler) - a load from `flattened_srt` at a uniform index counts as unwritten memory in `SpannedBarrierInDivergentFlow` when the emitter lists the variable and it is `NonWritable`. The rule rests on the driver filling that buffer from the resolved SRT words before dispatch and on no other binding aliasing it; neither is checked at run time, and a wrong proof hangs the device at a workgroup barrier. ShaderData, the BDA page table and the image table map are not listed. A barrier decided by a per-invocation value no memory rule makes uniform still falls back to two lanes per invocation. `APS5_NO_SPANNED_SNAPSHOT_LOADS` turns the rule off.

- [Spanned snapshot loads cost](../../../core/shader/recompiler/SpirvBackend/src/SpirvSpannedUniformity.cpp) (shader recompiler) - two programs that stop falling back through the snapshot rule (0x80002c4500 and 0x800037f200 in the Yotei kanji runs) run about 2x slower per dispatch as spanned waves than as two-lane waves, about 7-10 s per all-cold kanji run. The rule is kept because fewer fallbacks (152 to 34 distinct programs) saved more than that there; no per-program cost model decides it, so a title with many such programs could lose. Not measured on main.

