# libSceAgcDriver

## change [Native multisample color snapshots]

- [Native multisample color snapshots](../../../core/libs/prx/libSceAgcDriver/Graphics/src/MultisampleTarget.cpp) do not publish guest memory. Guest-footprint snapshots require uncompressed single-layer R_X and a retained initial footprint; low-level view creation without a captured baseline refuses. Raw sample readback does not support EQAA, arrays, volumes or mips. The baseline remains accounted for during the resident target's lifetime, while upload buffers expire after completion. Snapshots preserve padding and guards and reject unexpected guest changes, another active recorder on the same device, and completion reentry. Automatic consumers, console equivalence, unsynchronized guest writes and large-title costs remain unverified.

## add Functional

- [Native multisample color publication](../../../core/libs/prx/libSceAgcDriver/Graphics/src/MultisampleTarget.cpp) does not support automatic CPU or shader-buffer consumers, general metadata or depth publication. Unwatched memory, unsynchronized CPU stores during commit, console equivalence and title costs remain unverified. Explicit single-layer uncompressed R_X publication pins mappings, rejects overlapping pending writes or owners, compares bytes and stores changed runs before retirement. On watched memory CPU and driver/GPU write stamps are checked before readback, after completion and before commit; a reused owner retires without writeback. COMPRESSION is retained in target state and latched for the resident lifetime; raw snapshots and publication refuse it even without CMASK. Uniform guest contents can seed an exact clear; nonuniform compressed imports throw. Snapshot readback restrictions still apply.
