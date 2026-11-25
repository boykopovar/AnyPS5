# libSceAgcDriver

## change [Native multisample color snapshots]

- [Native multisample color snapshots](../../../core/libs/prx/libSceAgcDriver/Graphics/src/MultisampleTarget.cpp) do not publish guest memory. Guest-footprint snapshots require uncompressed single-layer R_X and a retained initial footprint; low-level view creation without a captured baseline refuses. Raw sample readback does not support EQAA, arrays, volumes or mips. The baseline remains accounted for during the resident target's lifetime, while upload buffers expire after completion. Snapshots preserve padding and guards and reject unexpected guest changes, another active recorder on the same device, and completion reentry. Automatic consumers, console equivalence, unsynchronized guest writes and large-title costs remain unverified.

## add Functional

- [Native multisample color publication](../../../core/libs/prx/libSceAgcDriver/Graphics/src/MultisampleTarget.cpp) excludes general metadata/depth publication, vertex/index reads, dynamic BDA and single-sample image aliases. Unwatched memory, unsynchronized CPU stores during commit, console equivalence, title costs and foreign native-depth composition remain unverified. Explicit and guarded CPU reads publish uncompressed single-layer R_X snapshots. Publication pins mappings, rejects pending writes/owners, compares bytes and commits changed runs before retirement. Watched CPU and driver/GPU stamps are checked before readback, after completion and before commit; reused owners retire without writeback. COMPRESSION is latched while resident; raw publication refuses it. Uniform contents can seed a clear; nonuniform compressed imports throw. Read-only shader buffers synchronize before upload, proof reuse and moved binding; writable aliases preflight.

- [Display reads](../../../core/libs/prx/libSceAgcDriver/Execution/src/DisplayBuffer.cpp) do not implement multisample scanout or resolve. The guest read guard now publishes eligible native owners, or refuses unsupported owners, before flush and decode through both pixel and raw-texel entry points. Console scanout equivalence remains unverified.
