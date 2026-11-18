# libSceCommonDialog

## change [PS5 3.20 export inventory]

- 13 additional pending imports in [InventoryExports.cpp](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp) are listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCommonDialog.c). Their signatures and behavior are unverified apart from what the Functional section states; the ones it does not name use the no-parameter unknown-signature convention and throw through `NotImplemented_nid_no_patch`, for import resolution only.

## remove [`_ZN3sce16CommonDialogUtil6ClientD0Ev`]
