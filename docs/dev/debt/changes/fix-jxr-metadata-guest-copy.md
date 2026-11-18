# libSceJxr

## add Functional

- [FreeDescMetadata](../../../core/libs/prx/libSceJxr/src/Metadata.cpp) reads the guest `MetadataVariant` via `GuestCopyIn_nid_no_patch` and checks/writes the cleared payload field via `GuestCopyOut_nid_no_patch` while preserving host `errno`, throwing `std::invalid_argument` on unreadable or unwritable guest records before releasing guest memory.
