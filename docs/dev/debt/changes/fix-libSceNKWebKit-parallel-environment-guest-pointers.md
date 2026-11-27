# libSceNKWebKit

## add Functional

- [ParallelEnvironment guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/ParallelEnvironment.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest pointers in `WTF::ParallelEnvironment` constructors and `execute` is unmeasured; constructors preflight writable guest storage under `GuestArena::HostWrite`, copy initialized field ranges through `GuestCopyOut_nid_no_patch` while preserving struct padding and rolling back acquired worker pool entries on failure, and `execute` copies `ParallelEnvironment` and its thread pointer table through `GuestCopyIn_nid_no_patch` and verifies unique live worker-pool membership before dispatching, calling `NotImplementedAt_nid_no_patch` on unmapped, unreadable, unwritable, or invalid worker pointers. Closes when: console behavior for invalid guest pointers in `WTF::ParallelEnvironment` exports is verified on hardware.
