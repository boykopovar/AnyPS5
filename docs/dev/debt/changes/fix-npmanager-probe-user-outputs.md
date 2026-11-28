# libSceNpManager

## add sce2np value classes

- [sce::np::User account id and GetUser outputs](../../../core/libs/prx/libSceNpManager/ValueClasses.cpp) (libSceNpManager) - `User::GetAccountId` clears its output and copies the user-service result out through `GuestCopyOut_nid_no_patch` (the service writes a host local), and `User::HasAccountId` and `User::GetUser` write their outputs the same way; returning `0x80550003` (the null-output error) for a non-null unwritable output is a host-safety fallback, and whether the console faults or returns an argument error on unmapped memory is unmeasured. The `User` object itself is still read in place.
