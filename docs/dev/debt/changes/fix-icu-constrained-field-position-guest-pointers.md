# libSceIcu

## add Functional

- [Constrained field position](../../../core/libs/prx/libSceIcu/ConstrainedFieldPosition.cpp) rejects unreadable or unwritable guest handle, index, and error pointers via `NotImplemented_nid_no_patch(__func__)` while preserving `errno`, keeping incoming error status short-circuiting and host-side position storage unchanged.
