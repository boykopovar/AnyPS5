# libSceNetCtl

## change [Additional callable exports]

- [Additional callable exports](../../../core/libs/prx/libSceNetCtl/Export.cpp) - 20 entries present in both version-labelled images are import-resolution declarations only. None is in the pinned public 3.20 catalogue. Neither label authenticates retail provenance. Signatures and behavior are unverified; parameterless `int` declarations are placeholders, not ABI contracts. `sceInetStatInit` is implemented as a forward to `sceNetCtlInit`: the 13.40 body is a 5-byte jump to the same internal entry that `sceNetCtlInit` exports, so it returns and initialises as that export. The other 19 entries throw through `NotImplemented_nid_no_patch`; unidentified names use literal NID aliases. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

# libSceSystemService

## change [USB storage aliases]

- [USB storage aliases](../../../core/libs/prx/libSceSystemService/PendingExports.cpp) and [USB exports](../../../core/libs/prx/libSceUsbStorage/Export.cpp): direct-jump signatures are matched to the supplied USB module. RequestMap/RequestMapWSB take six arguments; the two extra helper arguments are internal. IsExist takes a device, directory and byte output, with a separate boolean failure result. Callbacks receive an event pointer and user data. Device information writes 824 bytes, device lists hold eight 32-bit IDs, and mount-point outputs occupy 128 bytes. Event/device field meanings and initialization attributes remain unresolved. `sceSystemServiceUsbStorageRequestMapWSB` is a 5-byte jump to the PLT thunk whose GOT relocation names `sceUsbStorageRequestMapWSB`, so the registers reach that export unchanged: its 32-bit fourth argument and status output apply, and the 64-bit types here are the 3.20 inventory's unverified shape. The wrapper forwards the registers unchanged.
