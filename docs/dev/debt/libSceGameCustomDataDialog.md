# libSceGameCustomDataDialog technical debt

### Silent stubs

- [libSceGameCustomDataDialog](../../../core/libs/prx/libSceGameCustomDataDialog/Export.cpp) - the game custom data dialog (`sceGameCustomDataDialogOpenA` and `sceGameCustomDataDialogGetResultA`) runs the common-dialog state machine but shows nothing and reports the user as having cancelled, as there is no PSN game custom data. The signatures are from [Orbital's type database](https://github.com/AlexAltea/orbital/blob/b321fcdbd700cc02da1cb1ad25412781d5ec73c6/tools/ida/db_types.json); the result layout beyond the leading `int32_t` result code is not known, so nothing else is written.

### Unknown function info

- [sceGameCustomDataDialogGetResult and sceGameCustomDataDialogOpen](../../../core/libs/prx/libSceGameCustomDataDialog/Export.cpp) (libSceGameCustomDataDialog) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceGameCustomDataDialog.c), but Orbital has prototypes only for the `A` variants and no credible prototypes were found for these. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
