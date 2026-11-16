# libSceAgcDriver

## change [Native multisample color snapshots]

- [Native multisample color snapshots](../../../core/libs/prx/libSceAgcDriver/Graphics/src/MultisampleTarget.cpp) do not publish guest memory. Guest-footprint snapshots require uncompressed single-layer R_X and a retained initial footprint; low-level view creation without a captured baseline refuses. Raw sample readback does not support EQAA, arrays, volumes or mips. The baseline remains accounted for during the resident target's lifetime, while upload buffers expire after completion. Snapshots preserve padding and guards and reject unexpected guest changes, another active recorder on the same device, and completion reentry. Automatic consumers, console equivalence, unsynchronized guest writes and large-title costs remain unverified.

## add Functional

- [Native multisample color publication](../../../core/libs/prx/libSceAgcDriver/Graphics/src/MultisampleTarget.cpp) does not provide automatic CPU or shader-buffer publication, cached draw-recipe routing, general metadata or depth publication. Concurrent unsynchronized guest writes during commit, console equivalence and large-title costs remain unverified. Explicit uncompressed single-layer R_X publication pins readable registered mappings separately around each snapshot comparison, without keeping the pins across recorder completion. Commit pins writable non-retiring mappings, rejects pending writes and overlapping owners, rechecks original bytes and stores changed runs through write tracking before retiring the owner and guards. Snapshot readback restrictions still apply.
