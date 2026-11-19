# libSceAgcDriver

## change [Native multisample color snapshots]

- [Native multisample color snapshots](../../../core/libs/prx/libSceAgcDriver/Graphics/src/MultisampleTarget.cpp) do not publish guest memory. Guest-footprint snapshots require uncompressed single-layer R_X and a retained initial footprint; low-level view creation without a captured baseline refuses. Raw sample readback does not support EQAA, arrays, volumes or mips. The baseline remains accounted for during the resident target's lifetime, while upload buffers expire after completion. Snapshots preserve padding and guards and reject unexpected guest changes, another active recorder on the same device, and completion reentry. Automatic consumers, console equivalence, unsynchronized guest writes and large-title costs remain unverified.

## add Functional

- [Native multisample color publication](../../../core/libs/prx/libSceAgcDriver/Graphics/src/MultisampleTarget.cpp) does not support general metadata or depth publication, fixed-function vertex/index reads, dynamic BDA or single-sample image aliases. Unwatched memory, unsynchronized CPU stores during commit, console equivalence and title costs remain unverified. Explicit calls and guarded CPU reads publish single-layer uncompressed R_X snapshots. Publication pins mappings, rejects overlapping pending writes or owners, compares bytes and stores changed runs before retirement. On watched memory CPU and driver/GPU write stamps are checked before readback, after completion and before commit; a reused owner retires without writeback. COMPRESSION without CMASK throws at decode; compressed targets with metadata remain excluded. Read-only shader buffers synchronize before upload, proof reuse and moved binding; writable aliases preflight first. Foreign native-depth composition remains unqualified.

- [Display reads](../../../core/libs/prx/libSceAgcDriver/Execution/src/DisplayBuffer.cpp) do not implement multisample scanout or resolve. The guest read guard now publishes eligible native owners, or refuses unsupported owners, before flush and decode through both pixel and raw-texel entry points. Console scanout equivalence remains unverified.
