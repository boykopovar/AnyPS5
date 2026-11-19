# libSceAgcDriver

## change [Native multisample color snapshots]

- [Native multisample color snapshots](../../../core/libs/prx/libSceAgcDriver/Graphics/src/MultisampleTarget.cpp) do not publish guest memory. Guest-footprint snapshots require uncompressed single-layer R_X and a retained initial footprint; low-level view creation without a captured baseline refuses. Raw sample readback does not support EQAA, arrays, volumes or mips. The baseline remains accounted for during the resident target's lifetime, while upload buffers expire after completion. Snapshots preserve padding and guards and reject unexpected guest changes, another active recorder on the same device, and completion reentry. Automatic consumers, console equivalence, unsynchronized guest writes and large-title costs remain unverified.

## add Functional

- [Native multisample color publication](../../../core/libs/prx/libSceAgcDriver/Graphics/src/MultisampleTarget.cpp) does not provide automatic shader-buffer publication, cached draw-recipe routing, general metadata or depth publication. Concurrent unsynchronized guest writes during commit, console equivalence and large-title costs remain unverified. Explicit calls and the guest-memory read guard publish uncompressed single-layer R_X snapshots before replacement capture or a CPU copy. Publication pins readable registered mappings separately around each snapshot comparison, without keeping the pins across recorder completion. Commit pins writable non-retiring mappings, rejects pending writes and overlapping owners, rechecks original bytes and stores changed runs through write tracking before retiring the owner and guards. Snapshot readback restrictions still apply.

## change [Native multisample color publication]

- [Publication](../../../core/libs/prx/libSceAgcDriver/Graphics/src/MultisampleTarget.cpp) checks both collected CPU writes and overlapping driver/GPU stores before readback, after completion and immediately before commit on watched memory. Byte-identical later stores retire the old owner instead of republishing its samples; unwatched memory and unsynchronized CPU writes during commit retain the limitations above. Review findings by frankea in [#6995](https://github.com/boykopovar/AnyPS5/pull/6995#issuecomment-6994122585) prompted these checks and the missing-CMASK admission regression. [AMD PAL's register definition](https://github.com/GPUOpen-Drivers/pal/blob/c5e800072a32f68b6ccc4422936d96167c6e0728/src/core/hw/gfxip/gfx9/chip/gfx9_plus_merged_registers.h#L628-L642) identifies CB_COLOR_INFO.COMPRESSION at bit 14; decoding now refuses it without a CMASK address. Compressed targets with metadata remain excluded from raw publication; expanded CMASK is not newly supported.

## change [Display buffer reads]

- [Display reads](../../../core/libs/prx/libSceAgcDriver/Execution/src/DisplayBuffer.cpp) now invoke the guest read guard before flushing or copying the scanout bytes, including raw-texel capture reads. An eligible native color owner is published through the existing guard; an unsupported owner is refused. The alias regression checks the complete published guest footprint and retirement through both display entry points; it does not establish multisample scanout or resolve semantics. This follows frankea's [#6995 review](https://github.com/boykopovar/AnyPS5/pull/6995#issuecomment-6994122585).
