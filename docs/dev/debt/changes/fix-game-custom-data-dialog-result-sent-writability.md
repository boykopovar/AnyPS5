# libSceGameCustomDataDialog

## add Functional

- [GetResult](../../../core/libs/prx/libSceGameCustomDataDialog/Export.cpp) (libSceGameCustomDataDialog) - destination result and sent recipient list output pointers are probed with GuestWritable_nid_no_patch upfront with caller errno preserved; rejecting unmapped or unwritable pointer before modifying output fields is host safety fallback with console error ordering unmeasured.
